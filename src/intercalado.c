/**
 * @file    intercalado.c
 * @brief   Enfoque intercalado (stride): el proceso r prueba las llaves
 *          r, r + p, r + 2p, ... hasta agotar el espacio.
 *
 * Cada proceso barre llaves bajas y altas a la vez, así que, para cualquier
 * llave k, quien la tiene (k mod p) la alcanza en floor(k/p) + 1 iteraciones:
 * el speedup ya no depende de dónde cae la llave y queda cerca de p.
 * El arranque, la antena de aviso y el reporte vienen de search_mpi.
 *
 * Ejemplo:  mpirun -np 4 ./bin/intercalado -f data/texto.txt -k media -b 24
 */
#include <stdint.h>
#include <stdlib.h>

#include "search_mpi.h"

int main(int argc, char **argv)
{
    mpi_search_t search;

    int status = mpi_search_setup(&argc, &argv, "intercalado", 1, &search);
    if (status != SEARCH_READY) {
        return status == SEARCH_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    const uint64_t key_space = search.problem.key_space;
    const uint64_t stride = (uint64_t)search.size;

    found_listener_t listener;
    found_listener_start(&listener, &search);

    uint64_t iterations = 0;
    for (uint64_t key = (uint64_t)search.rank; key < key_space; key += stride) {
        if (found_listener_try_key(&listener, &search, key, &iterations)) {
            break;
        }
    }

    search_result_t result;
    found_listener_finish(&listener, &search, &result);
    mpi_search_report(&search, "intercalado", &result, iterations);

    mpi_search_release(&search);
    return EXIT_SUCCESS;
}
