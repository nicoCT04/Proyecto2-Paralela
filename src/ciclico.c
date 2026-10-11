/**
 * @file    ciclico.c
 * @brief   Enfoque bloque-cíclico: el espacio se parte en chunks de C = 2^c
 *          llaves y el proceso r recorre los chunks r, r + p, r + 2p, ...
 *          probando dentro de cada uno las llaves en orden.
 *
 * Es el punto medio entre el naive (un chunk enorme por proceso) y el
 * intercalado (chunks de 1 llave): conserva el reparto parejo del intercalado
 * y recorre llaves contiguas. Para una llave k en el chunk j = floor(k/C), la
 * halla el proceso j mod p tras floor(j/p)*C + (k mod C) + 1 iteraciones.
 * El arranque, la antena de aviso y el reporte vienen de search_mpi.
 *
 * Ejemplo:  mpirun -np 4 ./bin/ciclico -f data/texto.txt -k media -b 24 -c 16
 */
#include <stdint.h>
#include <stdlib.h>

#include "search_mpi.h"

int main(int argc, char **argv)
{
    mpi_search_t search;

    int status = mpi_search_setup(&argc, &argv, "ciclico", OPT_CHUNK, 1, &search);
    if (status != SEARCH_READY) {
        return status == SEARCH_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* c < bits (lo valida options), así que el espacio se parte en chunks exactos. */
    const uint64_t chunk_size  = (uint64_t)1 << search.opts.chunk_bits;
    const uint64_t chunk_count = search.problem.key_space >> search.opts.chunk_bits;
    const uint64_t stride      = (uint64_t)search.size;

    found_listener_t listener;
    found_listener_start(&listener, &search);

    /* Si hay menos chunks que procesos, los que sobran solo esperan el aviso. */
    uint64_t iterations = 0;
    int stop = 0;
    for (uint64_t chunk = (uint64_t)search.rank; chunk < chunk_count && !stop; chunk += stride) {
        const uint64_t first = chunk * chunk_size;
        for (uint64_t key = first; key < first + chunk_size; ++key) {
            if (found_listener_try_key(&listener, &search, key, &iterations)) {
                stop = 1;
                break;
            }
        }
    }

    search_result_t result;
    found_listener_finish(&listener, &search, &result);
    mpi_search_report(&search, "ciclico", &result, iterations);

    mpi_search_release(&search);
    return EXIT_SUCCESS;
}
