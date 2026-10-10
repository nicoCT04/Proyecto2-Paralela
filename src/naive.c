/**
 * @file    naive.c
 * @brief   Versión naive con MPI (la del bruteforce.c del curso): el espacio de
 *          llaves se parte en bloques contiguos, uno por proceso.
 *
 * Solo el proceso 0 lee el .txt y cifra; el resto recibe el problema por
 * MPI_Bcast. Quien halla la llave avisa a todos con MPI_Send, y cada proceso
 * la espera con un MPI_Irecv puesto antes de buscar y cerrado con MPI_Wait;
 * MPI_Test cada POLL_INTERVAL llaves corta la búsqueda en cuanto llega el aviso.
 *
 * Ejemplo:  mpirun -np 4 ./bin/naive -f data/texto.txt -k 123456
 *           mpirun -np 4 ./bin/naive -f data/texto.txt -k media -b 24
 */
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "des_utils.h"
#include "options.h"
#include "problem.h"

/** Etiqueta del mensaje de aviso {llave, proceso, iteraciones}. */
#define FOUND_TAG 1

/** Llave imposible que el proceso 0 envía como aviso cuando vence el límite de tiempo. */
#define TIMEOUT_SENTINEL UINT64_MAX

/** Posiciones del encabezado que el proceso 0 difunde antes del problema. */
enum { HDR_OK, HDR_KEY_BITS, HDR_CIPHER_LENGTH, HDR_PHRASE_LENGTH, HDR_TIME_LIMIT, HDR_COUNT };

/**
 * @brief  Reparte el problema del proceso 0 a todos los demás.
 *
 * El proceso 0 difunde primero un encabezado con `ok`: si sus parámetros o el
 * archivo fallaron, todos salen limpios sin que nadie quede esperando.
 *
 * @param  rank     Rango de este proceso.
 * @param  ok       (proceso 0) 1 si preparó el problema, 0 si no.
 * @param  opts     Parámetros (en el proceso 0 ya validados; en el resto se llena time_limit_s).
 * @param  problem  Problema (en el proceso 0 ya preparado; en el resto se reserva aquí).
 * @return 0 si todos tienen el problema, -1 si hay que salir.
 */
static int share_problem(int rank, int ok, program_options_t *opts, crack_problem_t *problem)
{
    uint64_t header[HDR_COUNT] = {0};

    if (rank == 0) {
        header[HDR_OK] = (uint64_t)ok;
        if (ok) {
            header[HDR_KEY_BITS]      = (uint64_t)problem->key_bits;
            header[HDR_CIPHER_LENGTH] = problem->cipher_length;
            header[HDR_PHRASE_LENGTH] = strlen(problem->phrase);
            header[HDR_TIME_LIMIT]    = (uint64_t)opts->time_limit_s;
        }
    }
    MPI_Bcast(header, HDR_COUNT, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    if (header[HDR_OK] == 0) {
        return -1;
    }

    if (rank != 0) {
        memset(problem, 0, sizeof(*problem));
        problem->key_bits      = (int)header[HDR_KEY_BITS];
        problem->key_space     = (uint64_t)1 << problem->key_bits;
        problem->cipher_length = (size_t)header[HDR_CIPHER_LENGTH];
        problem->cipher        = malloc(problem->cipher_length);
        problem->phrase        = malloc(header[HDR_PHRASE_LENGTH] + 1);
        opts->time_limit_s     = (int)header[HDR_TIME_LIMIT];
        if (problem->cipher == NULL || problem->phrase == NULL) {
            fprintf(stderr, "Error: sin memoria en el proceso %d.\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    MPI_Bcast(problem->cipher, (int)problem->cipher_length, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(problem->phrase, (int)header[HDR_PHRASE_LENGTH] + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
    return 0;
}

/**
 * @brief  Envía el aviso {llave, proceso, iteraciones} a todos los procesos.
 *
 * Incluye a quien lo envía, para que su propio MPI_Irecv también se complete.
 *
 * @param  msg   Mensaje de 3 enteros.
 * @param  size  Número de procesos.
 */
static void notify_all(const uint64_t msg[3], int size)
{
    for (int dest = 0; dest < size; ++dest) {
        MPI_Send(msg, 3, MPI_UINT64_T, dest, FOUND_TAG, MPI_COMM_WORLD);
    }
}

int main(int argc, char **argv)
{
    int rank, size;
    program_options_t opts;
    crack_problem_t problem;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* Solo el proceso 0 lee parámetros y archivo: los errores salen una vez. */
    int ok = 1;
    int status = OPTIONS_OK;
    memset(&opts, 0, sizeof(opts));
    memset(&problem, 0, sizeof(problem));
    if (rank == 0) {
        status = parse_options(argc, argv, "naive", 0, &opts);
        ok = status == OPTIONS_OK && problem_prepare(&opts, &problem) == 0;
    }
    if (share_problem(rank, ok, &opts, &problem) != 0) {
        MPI_Finalize();
        return status == OPTIONS_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if ((uint64_t)size > problem.key_space) {
        if (rank == 0) {
            fprintf(stderr, "Error: %d procesos son más que las 2^%d llaves del espacio.\n",
                    size, problem.key_bits);
        }
        problem_release(&problem);
        MPI_Finalize();
        return EXIT_FAILURE;
    }
    if (rank == 0) {
        problem_print_header("naive", &opts, &problem, size);
    }

    unsigned char *workspace = malloc(problem.cipher_length + 1);
    if (workspace == NULL) {
        fprintf(stderr, "Error: sin memoria en el proceso %d.\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    /* Reparto naive: bloques contiguos; el último absorbe el residuo. */
    uint64_t block = problem.key_space / (uint64_t)size;
    uint64_t lower = (uint64_t)rank * block;
    uint64_t upper = rank == size - 1 ? problem.key_space : lower + block;

    /* La "antena": recibe el aviso {llave, proceso, iteraciones} de quien la halle. */
    uint64_t found_msg[3] = {0, 0, 0};
    MPI_Request request;
    MPI_Irecv(found_msg, 3, MPI_UINT64_T, MPI_ANY_SOURCE, FOUND_TAG, MPI_COMM_WORLD, &request);

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    /*
     * Solo el proceso 0 vigila el límite de tiempo. Carrera conocida: si alguien
     * halla la llave justo cuando vence el límite, uno de los dos avisos queda
     * sin recibir al final; es inofensivo y casi imposible.
     */
    double deadline = rank == 0 && opts.time_limit_s > 0 ? start + opts.time_limit_s : 0.0;
    int timeout_sent = 0;
    int done = 0;

    uint64_t iterations = 0;
    for (uint64_t key = lower; key < upper; ++key) {
        if ((iterations & (POLL_INTERVAL - 1)) == 0) {
            /* Sin MPI_Test, Open MPI no avanza la recepción mientras se calcula. */
            MPI_Test(&request, &done, MPI_STATUS_IGNORE);
            if (done) {
                break;
            }
            if (deadline > 0.0 && !timeout_sent && MPI_Wtime() >= deadline) {
                const uint64_t timeout_msg[3] = {TIMEOUT_SENTINEL, 0, 0};
                notify_all(timeout_msg, size);
                timeout_sent = 1;
            }
        }
        ++iterations;
        if (try_key(key, problem.cipher, problem.cipher_length, problem.phrase, workspace)) {
            const uint64_t msg[3] = {key, (uint64_t)rank, iterations};
            notify_all(msg, size);
            break;
        }
    }

    /* El proceso 0 agotó su bloque: sigue vigilando el reloj mientras espera el aviso. */
    if (deadline > 0.0 && !timeout_sent) {
        const struct timespec pause = {0, 1000000};
        MPI_Test(&request, &done, MPI_STATUS_IGNORE);
        while (!done && MPI_Wtime() < deadline) {
            nanosleep(&pause, NULL);
            MPI_Test(&request, &done, MPI_STATUS_IGNORE);
        }
        if (!done) {
            const uint64_t timeout_msg[3] = {TIMEOUT_SENTINEL, 0, 0};
            notify_all(timeout_msg, size);
        }
    }

    MPI_Wait(&request, MPI_STATUS_IGNORE);
    double elapsed = MPI_Wtime() - start;

    uint64_t total_keys = 0;
    MPI_Reduce(&iterations, &total_keys, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        search_result_t result = {0};
        result.found       = found_msg[0] != TIMEOUT_SENTINEL;
        result.found_key   = result.found ? found_msg[0] : 0;
        result.finder_rank = result.found ? (int)found_msg[1] : -1;
        result.iterations  = result.found ? found_msg[2] : 0;
        result.total_keys  = total_keys;
        result.processes   = size;
        result.threads     = 1;
        result.elapsed_s   = elapsed;
        problem_print_result("naive", &opts, &problem, &result);
    }

    free(workspace);
    problem_release(&problem);
    MPI_Finalize();
    return EXIT_SUCCESS;
}
