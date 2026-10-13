/**
 * @file    hibrido.c
 * @brief   Enfoque híbrido MPI + OpenMP: el intercalado repartido entre
 *          p procesos y T hilos por proceso.
 *
 * El hilo t del proceso r es el trabajador global g = r·T + t y prueba las
 * llaves g, g + pT, g + 2pT, ...: es el intercalado con p·T trabajadores, así
 * que cualquier llave k cae en floor(k/pT) + 1 iteraciones del hilo que la
 * tiene. MPI se inicia con MPI_THREAD_FUNNELED: solo el hilo 0 de cada proceso
 * sondea la antena (MPI_Test) y el aviso se envía fuera de la región paralela.
 * Los hilos se toman de OMP_NUM_THREADS en el proceso 0 y se difunden a todos.
 *
 * Ejemplo:  OMP_NUM_THREADS=4 mpirun -x OMP_NUM_THREADS -np 4 ./bin/hibrido \
 *               -f data/texto.txt -k media -b 24
 */
#include <omp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "des_utils.h"
#include "search_mpi.h"

/** Máximo de hilos por proceso que se acepta. */
#define MAX_THREADS 256

/** Hallazgo de los hilos de un proceso, compartido dentro de la región paralela. */
typedef struct {
    atomic_int stop;        /**< 1 cuando hay que dejar de buscar (hallazgo propio o aviso). */
    int        found;       /**< 1 si un hilo de este proceso halló la llave. */
    uint64_t   key;         /**< Llave hallada. */
    uint64_t   iterations;  /**< Llaves que probó el hilo que la halló. */
    int        no_memory;   /**< 1 si algún hilo no pudo reservar su workspace. */
} thread_shared_t;

/**
 * @brief  Inicia MPI con soporte de hilos (FUNNELED) antes del arranque común.
 * @param  argc, argv  Argumentos del programa.
 * @return 0 si el MPI instalado lo soporta, -1 si no (el proceso 0 ya avisó y MPI ya se cerró).
 */
static int init_threaded_mpi(int *argc, char ***argv)
{
    int provided = MPI_THREAD_SINGLE;
    MPI_Init_thread(argc, argv, MPI_THREAD_FUNNELED, &provided);
    if (provided >= MPI_THREAD_FUNNELED) {
        return 0;
    }
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        fprintf(stderr, "Error: este MPI no soporta hilos (MPI_THREAD_FUNNELED).\n");
    }
    MPI_Finalize();
    return -1;
}

/**
 * @brief  Acuerda los hilos por proceso: los decide el proceso 0 y los difunde.
 * @param  search  Búsqueda ya preparada.
 * @return Hilos por proceso, o -1 si no sirven (el proceso 0 ya avisó).
 */
static int share_threads(const mpi_search_t *search)
{
    int threads = 0;
    if (search->rank == 0) {
        threads = omp_get_max_threads();
        uint64_t workers = (uint64_t)search->size * (uint64_t)threads;
        if (threads < 1 || threads > MAX_THREADS) {
            fprintf(stderr, "Error: OMP_NUM_THREADS debe estar entre 1 y %d (se pidieron %d).\n",
                    MAX_THREADS, threads);
            threads = -1;
        } else if (workers > search->problem.key_space) {
            fprintf(stderr, "Error: %llu trabajadores son más que las 2^%d llaves del espacio.\n",
                    (unsigned long long)workers, search->problem.key_bits);
            threads = -1;
        }
    }
    MPI_Bcast(&threads, 1, MPI_INT, 0, MPI_COMM_WORLD);
    return threads;
}

/**
 * @brief  Búsqueda de un hilo: intercalado con paso p·T desde su trabajador global.
 *
 * Cada POLL_INTERVAL llaves revisa la bandera compartida; el hilo 0 además
 * sondea la antena MPI y levanta la bandera si llegó un aviso.
 *
 * @param  listener  Antena del proceso (solo la toca el hilo 0).
 * @param  search    Búsqueda.
 * @param  threads   Hilos por proceso.
 * @param  shared    Bandera de parada y hallazgo del proceso.
 * @return Llaves que probó este hilo.
 */
static uint64_t search_thread(found_listener_t *listener, const mpi_search_t *search,
                              int threads, thread_shared_t *shared)
{
    const int thread = omp_get_thread_num();
    const crack_problem_t *problem = &search->problem;
    const uint64_t stride = (uint64_t)search->size * (uint64_t)threads;
    const uint64_t first = (uint64_t)search->rank * (uint64_t)threads + (uint64_t)thread;

    unsigned char *workspace = malloc(problem->cipher_length + 1);
    if (workspace == NULL) {
        shared->no_memory = 1;
        atomic_store(&shared->stop, 1);
        return 0;
    }

    uint64_t iterations = 0;
    for (uint64_t key = first; key < problem->key_space; key += stride) {
        if ((iterations & (POLL_INTERVAL - 1)) == 0) {
            if (thread == 0 && found_listener_poll(listener, search)) {
                atomic_store(&shared->stop, 1);
            }
            if (atomic_load_explicit(&shared->stop, memory_order_relaxed)) {
                break;
            }
        }
        ++iterations;
        if (try_key(key, problem->cipher, problem->cipher_length, problem->phrase, workspace)) {
            #pragma omp critical(found_key)
            if (!shared->found) {
                shared->found      = 1;
                shared->key        = key;
                shared->iterations = iterations;
            }
            atomic_store(&shared->stop, 1);
            break;
        }
    }

    free(workspace);
    return iterations;
}

int main(int argc, char **argv)
{
    if (init_threaded_mpi(&argc, &argv) != 0) {
        return EXIT_FAILURE;
    }

    mpi_search_t search;
    int status = mpi_search_setup(&argc, &argv, "hibrido", 0, 1, &search);
    if (status != SEARCH_READY) {
        return status == SEARCH_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const int threads = share_threads(&search);
    if (threads < 1) {
        mpi_search_release(&search);
        return EXIT_FAILURE;
    }
    if (search.rank == 0) {
        printf("Hilos por proceso: %d (%d trabajadores)\n", threads, search.size * threads);
        fflush(stdout);
    }

    thread_shared_t shared = {0};
    atomic_init(&shared.stop, 0);

    found_listener_t listener;
    found_listener_start(&listener, &search);

    uint64_t iterations = 0;
    #pragma omp parallel num_threads(threads) reduction(+ : iterations)
    iterations += search_thread(&listener, &search, threads, &shared);

    if (shared.no_memory) {
        fprintf(stderr, "Error: sin memoria en el proceso %d.\n", search.rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if (shared.found) {
        found_listener_announce(&search, shared.key, shared.iterations);
    }

    search_result_t result;
    found_listener_finish(&listener, &search, &result);
    result.threads = threads;
    mpi_search_report(&search, "hibrido", &result, iterations);

    mpi_search_release(&search);
    return EXIT_SUCCESS;
}
