/**
 * @file    search_mpi.c
 * @brief   Módulo común de las búsquedas con MPI (ver search_mpi.h).
 */
#include "search_mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "des_utils.h"

/** Etiqueta del mensaje de aviso {llave, proceso, iteraciones}. */
#define FOUND_TAG 1

/** Llave imposible que el proceso 0 envía como aviso cuando vence el límite de tiempo. */
#define TIMEOUT_SENTINEL UINT64_MAX

/** Posiciones del encabezado que el proceso 0 difunde antes del problema. */
enum {
    HDR_STATUS, HDR_KEY_BITS, HDR_CIPHER_LENGTH, HDR_PHRASE_LENGTH, HDR_TIME_LIMIT, HDR_CHUNK_BITS,
    HDR_COUNT
};

/**
 * @brief  (Proceso 0) Valida que el número de procesos sirva para el problema.
 * @param  size           Número de procesos.
 * @param  min_processes  Mínimo que necesita el programa.
 * @param  problem        Problema ya preparado.
 * @return 0 si sirve, -1 si no (con mensaje en stderr).
 */
static int validate_processes(int size, int min_processes, const crack_problem_t *problem)
{
    if (size < min_processes) {
        fprintf(stderr, "Error: este programa necesita al menos %d procesos (se pidieron %d).\n",
                min_processes, size);
        return -1;
    }
    if ((uint64_t)size > problem->key_space) {
        fprintf(stderr, "Error: %d procesos son más que las 2^%d llaves del espacio.\n",
                size, problem->key_bits);
        return -1;
    }
    return 0;
}

/**
 * @brief  Reparte el estado y el problema del proceso 0 a todos los demás.
 *
 * El proceso 0 difunde primero un encabezado con su estado: si pidieron -h o
 * algo falló, todos se enteran a la vez y salen limpios sin que nadie quede
 * esperando.
 *
 * @param  search  Búsqueda (en el proceso 0 ya preparada; en el resto se reserva aquí).
 * @param  status  (proceso 0) SEARCH_READY, SEARCH_HELP o SEARCH_ERROR.
 * @return El estado del proceso 0, igual en todos.
 */
static int share_problem(mpi_search_t *search, int status)
{
    crack_problem_t *problem = &search->problem;
    uint64_t header[HDR_COUNT] = {0};

    if (search->rank == 0) {
        header[HDR_STATUS] = (uint64_t)(int64_t)status;
        if (status == SEARCH_READY) {
            header[HDR_KEY_BITS]      = (uint64_t)problem->key_bits;
            header[HDR_CIPHER_LENGTH] = problem->cipher_length;
            header[HDR_PHRASE_LENGTH] = strlen(problem->phrase);
            header[HDR_TIME_LIMIT]    = (uint64_t)search->opts.time_limit_s;
            header[HDR_CHUNK_BITS]    = (uint64_t)search->opts.chunk_bits;
        }
    }
    MPI_Bcast(header, HDR_COUNT, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    status = (int)(int64_t)header[HDR_STATUS];
    if (status != SEARCH_READY) {
        return status;
    }

    if (search->rank != 0) {
        problem->key_bits          = (int)header[HDR_KEY_BITS];
        problem->key_space         = (uint64_t)1 << problem->key_bits;
        problem->cipher_length     = (size_t)header[HDR_CIPHER_LENGTH];
        problem->cipher            = malloc(problem->cipher_length);
        problem->phrase            = malloc(header[HDR_PHRASE_LENGTH] + 1);
        search->opts.time_limit_s  = (int)header[HDR_TIME_LIMIT];
        search->opts.chunk_bits    = (int)header[HDR_CHUNK_BITS];
        if (problem->cipher == NULL || problem->phrase == NULL) {
            fprintf(stderr, "Error: sin memoria en el proceso %d.\n", search->rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    MPI_Bcast(problem->cipher, (int)problem->cipher_length, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(problem->phrase, (int)header[HDR_PHRASE_LENGTH] + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
    return SEARCH_READY;
}

int mpi_search_setup(int *argc, char ***argv, const char *program, int option_flags,
                     int min_processes, mpi_search_t *search)
{
    memset(search, 0, sizeof(*search));
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &search->rank);
    MPI_Comm_size(MPI_COMM_WORLD, &search->size);

    /* Solo el proceso 0 lee parámetros y archivo: los errores salen una vez. */
    int status = SEARCH_READY;
    if (search->rank == 0) {
        int parsed = parse_options(*argc, *argv, program, option_flags, &search->opts);
        if (parsed == OPTIONS_HELP) {
            status = SEARCH_HELP;
        } else if (parsed != OPTIONS_OK
                   || problem_prepare(&search->opts, &search->problem) != 0
                   || validate_processes(search->size, min_processes, &search->problem) != 0) {
            status = SEARCH_ERROR;
        }
    }

    status = share_problem(search, status);
    if (status != SEARCH_READY) {
        problem_release(&search->problem);
        MPI_Finalize();
        return status;
    }

    if (search->rank == 0) {
        problem_print_header(program, &search->opts, &search->problem, search->size);
    }
    search->workspace = malloc(search->problem.cipher_length + 1);
    if (search->workspace == NULL) {
        fprintf(stderr, "Error: sin memoria en el proceso %d.\n", search->rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    return SEARCH_READY;
}

void mpi_search_release(mpi_search_t *search)
{
    free(search->workspace);
    search->workspace = NULL;
    problem_release(&search->problem);
    MPI_Finalize();
}

double mpi_search_begin(void)
{
    MPI_Barrier(MPI_COMM_WORLD);
    return MPI_Wtime();
}

/**
 * @brief  Envía un aviso de 3 enteros a todos los procesos.
 *
 * Incluye a quien lo envía, para que su propio MPI_Irecv también se complete.
 *
 * @param  message  Mensaje {llave, proceso, iteraciones}.
 * @param  size     Número de procesos.
 */
static void notify_all(const uint64_t message[3], int size)
{
    for (int dest = 0; dest < size; ++dest) {
        MPI_Send(message, 3, MPI_UINT64_T, dest, FOUND_TAG, MPI_COMM_WORLD);
    }
}

/**
 * @brief  (Proceso 0) Avisa a todos que venció el límite de tiempo.
 * @param  listener  Antena (se marca timeout_sent).
 * @param  size      Número de procesos.
 */
static void announce_timeout(found_listener_t *listener, int size)
{
    const uint64_t timeout_message[3] = {TIMEOUT_SENTINEL, 0, 0};
    notify_all(timeout_message, size);
    listener->timeout_sent = 1;
}

void found_listener_start(found_listener_t *listener, const mpi_search_t *search)
{
    memset(listener, 0, sizeof(*listener));
    MPI_Irecv(listener->message, 3, MPI_UINT64_T, MPI_ANY_SOURCE, FOUND_TAG, MPI_COMM_WORLD,
              &listener->request);
    listener->start = mpi_search_begin();

    /*
     * Solo el proceso 0 vigila el límite de tiempo. Carrera conocida: si alguien
     * halla la llave justo cuando vence el límite, uno de los dos avisos queda
     * sin recibir al final; es inofensivo y casi imposible.
     */
    if (search->rank == 0 && search->opts.time_limit_s > 0) {
        listener->deadline = listener->start + search->opts.time_limit_s;
    }
}

int found_listener_poll(found_listener_t *listener, const mpi_search_t *search)
{
    /* Sin MPI_Test, Open MPI no avanza la recepción mientras se calcula. */
    MPI_Test(&listener->request, &listener->done, MPI_STATUS_IGNORE);
    if (listener->done) {
        return 1;
    }
    if (listener->deadline > 0.0 && !listener->timeout_sent && MPI_Wtime() >= listener->deadline) {
        announce_timeout(listener, search->size);
    }
    return 0;
}

void found_listener_announce(const mpi_search_t *search, uint64_t key, uint64_t iterations)
{
    const uint64_t message[3] = {key, (uint64_t)search->rank, iterations};
    notify_all(message, search->size);
}

int found_listener_try_key(found_listener_t *listener, const mpi_search_t *search,
                           uint64_t key, uint64_t *iterations)
{
    if ((*iterations & (POLL_INTERVAL - 1)) == 0 && found_listener_poll(listener, search)) {
        return 1;
    }
    ++*iterations;
    if (try_key(key, search->problem.cipher, search->problem.cipher_length,
                search->problem.phrase, search->workspace)) {
        found_listener_announce(search, key, *iterations);
        return 1;
    }
    return 0;
}

void found_listener_finish(found_listener_t *listener, const mpi_search_t *search,
                           search_result_t *result)
{
    /* El proceso 0 agotó su parte: sigue vigilando el reloj mientras espera el aviso. */
    if (listener->deadline > 0.0 && !listener->timeout_sent) {
        const struct timespec pause = {0, 1000000};
        MPI_Test(&listener->request, &listener->done, MPI_STATUS_IGNORE);
        while (!listener->done && MPI_Wtime() < listener->deadline) {
            nanosleep(&pause, NULL);
            MPI_Test(&listener->request, &listener->done, MPI_STATUS_IGNORE);
        }
        if (!listener->done) {
            announce_timeout(listener, search->size);
        }
    }

    MPI_Wait(&listener->request, MPI_STATUS_IGNORE);
    double elapsed = MPI_Wtime() - listener->start;

    memset(result, 0, sizeof(*result));
    result->found       = listener->message[0] != TIMEOUT_SENTINEL;
    result->found_key   = result->found ? listener->message[0] : 0;
    result->finder_rank = result->found ? (int)listener->message[1] : -1;
    result->iterations  = result->found ? listener->message[2] : 0;
    result->processes   = search->size;
    result->threads     = 1;
    result->elapsed_s   = elapsed;
}

void mpi_search_report(const mpi_search_t *search, const char *program,
                       const search_result_t *result, uint64_t own_iterations)
{
    uint64_t total_keys = 0;
    MPI_Reduce(&own_iterations, &total_keys, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    if (search->rank == 0) {
        search_result_t final_result = *result;
        final_result.total_keys = total_keys;
        problem_print_result(program, &search->opts, &search->problem, &final_result);
    }
}
