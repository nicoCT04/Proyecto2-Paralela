/**
 * @file    search_mpi.h
 * @brief   Módulo común de las búsquedas con MPI: arranque, aviso de llave y reporte.
 *
 * Todos los programas MPI arrancan igual: solo el proceso 0 lee los parámetros
 * y el .txt, y difunde el problema con MPI_Bcast. Los de reparto estático
 * (naive, intercalado, bloque-cíclico) comparten además la "antena" de aviso:
 * un MPI_Irecv puesto antes de buscar, MPI_Send a todos cuando alguien halla
 * la llave y MPI_Test cada POLL_INTERVAL llaves. Lo único que cambia entre
 * ellos es qué llaves prueba cada proceso y en qué orden.
 *
 * Uso típico:
 *   mpi_search_setup -> found_listener_start -> bucle con found_listener_try_key
 *   -> found_listener_finish -> mpi_search_report -> mpi_search_release
 */
#ifndef SEARCH_MPI_H
#define SEARCH_MPI_H

#include <mpi.h>
#include <stdint.h>

#include "options.h"
#include "problem.h"

/** Resultado de mpi_search_setup. */
enum {
    SEARCH_READY = 0,   /**< Todo listo: se puede buscar. */
    SEARCH_HELP  = 1,   /**< Se pidió -h: la ayuda ya se imprimió y MPI ya se cerró. */
    SEARCH_ERROR = -1   /**< Parámetros, archivo o procesos inválidos: ya se avisó y MPI ya se cerró. */
};

/** Estado compartido de una búsqueda MPI, igual en todos los procesos. */
typedef struct {
    int               rank;       /**< Rango de este proceso. */
    int               size;       /**< Número de procesos. */
    program_options_t opts;       /**< Parámetros; completos solo en el proceso 0
                                       (time_limit_s y chunk_bits en todos). */
    crack_problem_t   problem;    /**< Cifrado, frase y espacio; el texto plano solo en el proceso 0. */
    unsigned char    *workspace;  /**< Buffer de try_key, reservado una sola vez. */
} mpi_search_t;

/** Antena de aviso de "llave encontrada" de los repartos estáticos. */
typedef struct {
    MPI_Request request;       /**< El MPI_Irecv puesto antes de buscar. */
    uint64_t    message[3];    /**< {llave, proceso que la halló, sus iteraciones}. */
    int         done;          /**< 1 cuando el aviso ya llegó. */
    double      start;         /**< Inicio de la búsqueda (MPI_Wtime tras la barrera). */
    double      deadline;      /**< Fin del límite de tiempo (solo el proceso 0; 0 = sin límite). */
    int         timeout_sent;  /**< 1 si el proceso 0 ya mandó el aviso de límite vencido. */
} found_listener_t;

/**
 * @brief  Inicia MPI, reparte el problema y deja todo listo para buscar.
 *
 * Llama a MPI_Init si MPI no se inició antes (el híbrido lo inicia con
 * MPI_Init_thread). El proceso 0 lee parámetros y archivo y valida el número
 * de procesos; el resto recibe el problema por MPI_Bcast. Los errores salen una sola vez
 * (proceso 0) y, si no hay que buscar, todos cierran MPI sin quedar esperando.
 *
 * @param  argc, argv     Argumentos del programa (se pasan a MPI_Init).
 * @param  program        Nombre del programa (para la ayuda y el reporte).
 * @param  option_flags   Opciones extra que admite (0 u OPT_CHUNK; ver options.h).
 * @param  min_processes  Mínimo de procesos que necesita el programa.
 * @param  search         Salida: estado de la búsqueda (liberar con mpi_search_release).
 * @return SEARCH_READY, SEARCH_HELP o SEARCH_ERROR.
 */
int mpi_search_setup(int *argc, char ***argv, const char *program, int option_flags,
                     int min_processes, mpi_search_t *search);

/**
 * @brief  Libera la memoria de la búsqueda y cierra MPI.
 * @param  search  Búsqueda preparada con mpi_search_setup.
 */
void mpi_search_release(mpi_search_t *search);

/**
 * @brief  Sincroniza a todos (MPI_Barrier) y marca el inicio de la búsqueda.
 * @return MPI_Wtime justo después de la barrera.
 */
double mpi_search_begin(void);

/**
 * @brief  Pone la antena (MPI_Irecv), sincroniza y arranca el reloj.
 * @param  listener  Salida: antena lista.
 * @param  search    Búsqueda (el proceso 0 toma de aquí el límite de tiempo).
 */
void found_listener_start(found_listener_t *listener, const mpi_search_t *search);

/**
 * @brief  Revisa si ya llegó el aviso; el proceso 0 además vigila el límite de tiempo.
 * @param  listener  Antena.
 * @param  search    Búsqueda.
 * @return 1 si ya llegó un aviso y hay que dejar de buscar, 0 si no.
 */
int found_listener_poll(found_listener_t *listener, const mpi_search_t *search);

/**
 * @brief  Envía el aviso {llave, proceso, iteraciones} a todos, incluido uno mismo.
 * @param  search      Búsqueda.
 * @param  key         Llave hallada.
 * @param  iterations  Llaves que probó este proceso hasta hallarla.
 */
void found_listener_announce(const mpi_search_t *search, uint64_t key, uint64_t iterations);

/**
 * @brief  Un paso de la búsqueda: sondea cada POLL_INTERVAL llaves, prueba `key`
 *         y avisa a todos si es la correcta.
 * @param  listener    Antena.
 * @param  search      Búsqueda.
 * @param  key         Llave candidata.
 * @param  iterations  Entrada y salida: llaves probadas por este proceso.
 * @return 1 si hay que dejar de buscar (la halló este proceso u otro avisó), 0 si no.
 */
int found_listener_try_key(found_listener_t *listener, const mpi_search_t *search,
                           uint64_t key, uint64_t *iterations);

void found_listener_progress(const found_listener_t *listener, const mpi_search_t *search,
                             int worker, uint64_t iterations);

/**
 * @brief  Espera el aviso (el proceso 0 sigue vigilando el límite) y arma el resultado.
 * @param  listener  Antena.
 * @param  search    Búsqueda.
 * @param  result    Salida: hallazgo, proceso, iteraciones y tiempo (sin total_keys).
 */
void found_listener_finish(found_listener_t *listener, const mpi_search_t *search,
                           search_result_t *result);

/**
 * @brief  Suma las llaves probadas por todos (MPI_Reduce) y el proceso 0 imprime el reporte.
 * @param  search          Búsqueda.
 * @param  program         Nombre del programa.
 * @param  result          Resultado (solo se usa en el proceso 0).
 * @param  own_iterations  Llaves que probó este proceso.
 */
void mpi_search_report(const mpi_search_t *search, const char *program,
                       const search_result_t *result, uint64_t own_iterations);

#endif /* SEARCH_MPI_H */
