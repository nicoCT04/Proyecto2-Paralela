/**
 * @file    problem.h
 * @brief   El "problema" de una corrida: texto cargado, cifrado y frase clave,
 *          más las rutinas para reportar resultados de forma uniforme.
 *
 * Los programas de búsqueda imprimen al final una línea que empieza con
 * "CSV," para que la bitácora de pruebas se arme con un simple
 * `grep '^CSV,'`. Columnas:
 *   CSV,programa,procesos,hilos,bits,llave_real,llave_hallada,proceso,iteraciones,
 *       llaves_totales,tiempo_s
 * Si no se halló la llave: llave_hallada = NA, proceso = -1, iteraciones = 0.
 */
#ifndef PROBLEM_H
#define PROBLEM_H

#include <stddef.h>
#include <stdint.h>

#include "options.h"

/** Tamaño máximo del texto a cifrar: más texto solo vuelve más lento cada intento. */
#define MAX_TEXT_BYTES (64 * 1024)

/** Cada cuántas llaves se revisan los mensajes MPI y el límite de tiempo (potencia de 2). */
#define POLL_INTERVAL 4096

/** Datos que necesita la búsqueda de la llave. */
typedef struct {
    char          *plain_text;     /**< Texto original leído del .txt. */
    size_t         text_length;    /**< Bytes útiles del texto (sin relleno). */
    unsigned char *cipher;         /**< Texto cifrado, con relleno de ceros hasta múltiplo de 8. */
    size_t         cipher_length;  /**< Bytes de `cipher`. */
    char          *phrase;         /**< Copia propia de la frase clave. */
    int            key_bits;       /**< Bits del espacio de búsqueda. */
    uint64_t       key_space;      /**< Número de llaves posibles: 2^key_bits. */
} crack_problem_t;

/**
 * @brief  Carga el .txt, valida que contenga la frase clave y lo cifra con la llave.
 * @param  opts     Parámetros ya validados.
 * @param  problem  Salida: problema listo para la búsqueda (liberar con problem_release).
 * @return 0 si todo salió bien, -1 si no (con mensaje en stderr).
 */
int problem_prepare(const program_options_t *opts, crack_problem_t *problem);

/**
 * @brief  Libera la memoria del problema y deja sus punteros en NULL.
 * @param  problem  Problema a liberar (puede estar a medio construir).
 */
void problem_release(crack_problem_t *problem);

/**
 * @brief  Imprime el encabezado de la corrida: archivo, frase, llave y espacio.
 * @param  program    Nombre del programa.
 * @param  opts       Parámetros de la corrida.
 * @param  problem    Problema preparado.
 * @param  processes  Número de procesos.
 */
void problem_print_header(const char *program, const program_options_t *opts,
                          const crack_problem_t *problem, int processes);

/** Resultado de una búsqueda tal como lo reporta el proceso 0. */
typedef struct {
    int      found;        /**< 1 si se halló la llave, 0 si se agotó el límite de tiempo. */
    uint64_t found_key;    /**< Llave hallada (solo si found). */
    int      finder_rank;  /**< Proceso que la halló (-1 si no se halló). */
    uint64_t iterations;   /**< Llaves que probó ese proceso hasta hallarla. */
    uint64_t total_keys;   /**< Llaves probadas entre todos los procesos e hilos. */
    int      processes;    /**< Procesos MPI (1 en el secuencial). */
    int      threads;      /**< Hilos por proceso (1 sin OpenMP). */
    double   elapsed_s;    /**< Tiempo de pared de la búsqueda, en segundos. */
} search_result_t;

/**
 * @brief  Imprime el resultado de la búsqueda y la línea CSV para la bitácora.
 * @param  program  Nombre del programa.
 * @param  opts     Parámetros de la corrida (para la llave real y el límite de tiempo).
 * @param  problem  Problema (se usa para descifrar y mostrar el texto).
 * @param  result   Resultado de la búsqueda.
 */
void problem_print_result(const char *program, const program_options_t *opts,
                          const crack_problem_t *problem, const search_result_t *result);

/**
 * @brief  Imprime un buffer en hexadecimal, de 8 en 8 bytes (un bloque DES).
 * @param  label   Etiqueta de la línea.
 * @param  bytes   Datos.
 * @param  length  Bytes a imprimir.
 */
void print_hex(const char *label, const unsigned char *bytes, size_t length);

/** @brief Reloj de pared monotónico, en segundos. */
double wall_seconds(void);

#endif /* PROBLEM_H */
