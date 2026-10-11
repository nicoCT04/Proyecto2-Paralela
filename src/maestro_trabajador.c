/**
 * @file    maestro_trabajador.c
 * @brief   Enfoque maestro-trabajador: el proceso 0 reparte chunks de 2^c llaves
 *          bajo demanda y los procesos 1..p-1 los prueban.
 *
 * El maestro no busca: atiende pedidos. Cada trabajador pide un chunk, lo
 * prueba y pide otro; si halla la llave, la manda en su siguiente pedido.
 * Desde ese momento (o al vencer el límite, o al acabarse los chunks) el
 * maestro responde STOP a cada pedido. Invariante del protocolo: cada pedido
 * recibe exactamente una respuesta, así que nadie se cuelga en MPI_Finalize.
 *
 * Con W = p - 1 trabajadores el speedup ronda p - 1, pero el reparto se
 * balancea solo: un núcleo más lento simplemente pide menos chunks.
 * El límite de tiempo se respeta con resolución de un chunk.
 *
 * Ejemplo:  mpirun -np 4 ./bin/maestro_trabajador -f data/texto.txt -k media -b 24 -c 16
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "des_utils.h"
#include "search_mpi.h"

/** El proceso que reparte. */
#define MASTER 0

/** Etiquetas: pedido del trabajador, chunk asignado y orden de parar. */
enum { TAG_REQUEST = 10, TAG_WORK = 11, TAG_STOP = 12 };

/** Pedido de un trabajador: {estado, llave hallada, sus iteraciones}. */
enum { REQ_STATE, REQ_KEY, REQ_ITERATIONS, REQ_COUNT };

/** Estado del pedido: solo pide trabajo, o además avisa que halló la llave. */
enum { STATE_ASK = 0, STATE_FOUND = 1 };

/** Respuesta del maestro: {primera llave, una más allá de la última}. */
enum { WORK_FIRST, WORK_END, WORK_COUNT };

/**
 * @brief  (Proceso 0) Atiende pedidos hasta que todos los trabajadores reciben STOP.
 *
 * T_p se toma en el instante en que llega el aviso de la llave, no al final:
 * lo que tarden los demás en terminar su chunk ya no es parte de la búsqueda.
 *
 * @param  search  Búsqueda.
 * @param  result  Salida: hallazgo, proceso, iteraciones y tiempo (sin total_keys).
 */
static void run_master(const mpi_search_t *search, search_result_t *result)
{
    const uint64_t chunk_size  = (uint64_t)1 << search->opts.chunk_bits;
    const uint64_t chunk_count = search->problem.key_space >> search->opts.chunk_bits;
    uint64_t next_chunk = 0;
    int active = search->size - 1;
    int stopping = 0;

    memset(result, 0, sizeof(*result));
    result->finder_rank = -1;
    result->processes   = search->size;
    result->threads     = 1;

    const double start = mpi_search_begin();
    const double deadline = search->opts.time_limit_s > 0 ? start + search->opts.time_limit_s : 0.0;

    while (active > 0) {
        uint64_t request[REQ_COUNT];
        MPI_Status status;
        /* El maestro solo puede actuar al responder, así que basta esperar el pedido. */
        MPI_Recv(request, REQ_COUNT, MPI_UINT64_T, MPI_ANY_SOURCE, TAG_REQUEST, MPI_COMM_WORLD,
                 &status);

        if (request[REQ_STATE] == STATE_FOUND && !result->found) {
            result->found       = 1;
            result->found_key   = request[REQ_KEY];
            result->finder_rank = status.MPI_SOURCE;
            result->iterations  = request[REQ_ITERATIONS];
            result->elapsed_s   = MPI_Wtime() - start;
            stopping = 1;
        }
        if (!stopping && deadline > 0.0 && MPI_Wtime() >= deadline) {
            stopping = 1;
        }

        if (stopping || next_chunk >= chunk_count) {
            const uint64_t none[WORK_COUNT] = {0, 0};
            MPI_Send(none, WORK_COUNT, MPI_UINT64_T, status.MPI_SOURCE, TAG_STOP, MPI_COMM_WORLD);
            --active;
        } else {
            const uint64_t work[WORK_COUNT] = {next_chunk * chunk_size, (next_chunk + 1) * chunk_size};
            MPI_Send(work, WORK_COUNT, MPI_UINT64_T, status.MPI_SOURCE, TAG_WORK, MPI_COMM_WORLD);
            ++next_chunk;
        }
    }

    if (!result->found) {
        result->elapsed_s = MPI_Wtime() - start;
    }
}

/**
 * @brief  (Procesos 1..p-1) Pide chunks y los prueba hasta recibir STOP.
 * @param  search  Búsqueda.
 * @return Llaves que probó este trabajador.
 */
static uint64_t run_worker(const mpi_search_t *search)
{
    uint64_t iterations = 0;
    uint64_t request[REQ_COUNT] = {STATE_ASK, 0, 0};

    mpi_search_begin();
    for (;;) {
        uint64_t work[WORK_COUNT];
        MPI_Status status;
        MPI_Send(request, REQ_COUNT, MPI_UINT64_T, MASTER, TAG_REQUEST, MPI_COMM_WORLD);
        MPI_Recv(work, WORK_COUNT, MPI_UINT64_T, MASTER, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == TAG_STOP) {
            break;
        }

        for (uint64_t key = work[WORK_FIRST]; key < work[WORK_END]; ++key) {
            ++iterations;
            if (try_key(key, search->problem.cipher, search->problem.cipher_length,
                        search->problem.phrase, search->workspace)) {
                /* El aviso viaja en el siguiente pedido; la respuesta será STOP. */
                request[REQ_STATE]      = STATE_FOUND;
                request[REQ_KEY]        = key;
                request[REQ_ITERATIONS] = iterations;
                break;
            }
        }
    }
    return iterations;
}

int main(int argc, char **argv)
{
    mpi_search_t search;

    int status = mpi_search_setup(&argc, &argv, "maestro_trabajador", OPT_CHUNK, 2, &search);
    if (status != SEARCH_READY) {
        return status == SEARCH_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    search_result_t result;
    uint64_t own_iterations = 0;
    if (search.rank == MASTER) {
        run_master(&search, &result);
    } else {
        own_iterations = run_worker(&search);
    }
    mpi_search_report(&search, "maestro_trabajador", &result, own_iterations);

    mpi_search_release(&search);
    return EXIT_SUCCESS;
}
