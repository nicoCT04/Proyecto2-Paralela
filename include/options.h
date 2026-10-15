/**
 * @file    options.h
 * @brief   Lectura y validación de los parámetros de línea de comandos.
 *
 * Todos los programas comparten la misma interfaz:
 *   -f <archivo.txt>   texto plano a cifrar (obligatorio)
 *   -k <llave>         llave privada: número (decimal o 0x hex) o preset
 *                      (a, b, c, facil, media, dificil)
 *   -p <frase>         frase clave a buscar (por defecto "es una prueba de")
 *   -b <bits>          tamaño del espacio de llaves, 2^bits (8..56, por defecto 56)
 *   -t <segundos>      límite de tiempo de la búsqueda (0 = sin límite)
 *   -c <bits>          (solo `ciclico` y `maestro_trabajador`) chunk de 2^bits llaves
 *   -o <archivo>       (solo `cifrar`) guarda el texto cifrado en binario
 */
#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdint.h>

#define DEFAULT_PHRASE   "es una prueba de"
#define MIN_KEY_BITS     8
#define MAX_PHRASE_BYTES 256
#define MAX_TIME_LIMIT_S 86400

#define DEFAULT_CHUNK_BITS 16
#define MIN_CHUNK_BITS     8
#define MAX_CHUNK_BITS     30

/** Opciones extra que admite cada programa (se combinan con |). */
enum {
    OPT_OUTPUT = 1 << 0,   /**< -o: guardar el cifrado (cifrar). */
    OPT_CHUNK  = 1 << 1,   /**< -c: tamaño de chunk (ciclico, maestro_trabajador). */
    OPT_PROGRESS = 1 << 2
};

/** Resultado de parse_options. */
enum {
    OPTIONS_OK    = 0,   /**< Parámetros válidos: se puede continuar. */
    OPTIONS_HELP  = 1,   /**< Se pidió -h: se imprimió la ayuda y hay que salir con éxito. */
    OPTIONS_ERROR = -1   /**< Parámetros inválidos: ya se imprimió el motivo. */
};

/** Parámetros ya validados de una corrida. */
typedef struct {
    const char *text_path;    /**< Ruta del .txt con el texto plano. */
    const char *key_text;     /**< La llave tal como se escribió (-k). */
    uint64_t    key;          /**< La llave ya resuelta a número. */
    const char *phrase;       /**< Frase clave a buscar. */
    int         key_bits;     /**< Bits del espacio de búsqueda. */
    int         time_limit_s; /**< Límite de la búsqueda en segundos (0 = sin límite). */
    int         chunk_bits;   /**< Tamaño de chunk: 2^chunk_bits llaves (con OPT_CHUNK). */
    const char *output_path;  /**< Archivo de salida del cifrado (puede ser NULL). */
    int         progress;
} program_options_t;

/**
 * @brief  Lee y valida argv.
 * @param  argc, argv  Argumentos del programa.
 * @param  program     Nombre del programa (para la ayuda).
 * @param  flags       Opciones extra que admite: 0 o una combinación de OPT_OUTPUT y OPT_CHUNK.
 * @param  opts        Salida: parámetros validados.
 * @return OPTIONS_OK, OPTIONS_HELP u OPTIONS_ERROR.
 */
int parse_options(int argc, char **argv, const char *program,
                  int flags, program_options_t *opts);

/**
 * @brief  Convierte el texto de -k en una llave numérica.
 *
 * Presets (N = 2^bits):  a = 123456,  b = N/4 - 1,  c = N/4,
 *   facil = N/2 + 1,  media = N/2 + N/8,  dificil = ceil(N/7 + N/13).
 *
 * @param  text      Texto de la llave.
 * @param  key_bits  Bits del espacio de búsqueda.
 * @param  key_out   Salida: la llave.
 * @return 0 si es válida y cabe en el espacio, -1 si no (con mensaje en stderr).
 */
int resolve_key(const char *text, int key_bits, uint64_t *key_out);

#endif /* OPTIONS_H */
