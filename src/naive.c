/**
 * @file    naive.c
 * @brief   Versión naive con MPI (la del bruteforce.c del curso): el espacio de
 *          llaves se parte en bloques contiguos, uno por proceso.
 *
 * El arranque (MPI_Bcast del problema), la antena de aviso (MPI_Irecv,
 * MPI_Send a todos, MPI_Test cada POLL_INTERVAL llaves y MPI_Wait) y el
 * reporte vienen del módulo común search_mpi; aquí solo vive el reparto.
 *
 * Ejemplo:  mpirun -np 4 ./bin/naive -f data/texto.txt -k 123456
 *           mpirun -np 4 ./bin/naive -f data/texto.txt -k media -b 24
 */
#include <stdint.h>
#include <stdlib.h>

#include "search_mpi.h"

int main(int argc, char **argv)
{
    mpi_search_t search;

    int status = mpi_search_setup(&argc, &argv, "naive", 0, 1, &search);
    if (status != SEARCH_READY) {
        return status == SEARCH_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* Reparto naive: bloques contiguos; el último absorbe el residuo. */
    const uint64_t key_space = search.problem.key_space;
    const uint64_t block = key_space / (uint64_t)search.size;
    const uint64_t lower = (uint64_t)search.rank * block;
    const uint64_t upper = search.rank == search.size - 1 ? key_space : lower + block;

    found_listener_t listener;
    found_listener_start(&listener, &search);

    uint64_t iterations = 0;
    for (uint64_t key = lower; key < upper; ++key) {
        if (found_listener_try_key(&listener, &search, key, &iterations)) {
            break;
        }
    }
    found_listener_progress(&listener, &search, search.rank, iterations);

    search_result_t result;
    found_listener_finish(&listener, &search, &result);
    mpi_search_report(&search, "naive", &result, iterations);

    mpi_search_release(&search);
    return EXIT_SUCCESS;
}
