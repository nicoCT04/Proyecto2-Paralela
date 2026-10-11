/**
 * @file    options.c
 * @brief   Lectura y validación de parámetros (ver options.h).
 */
#include "options.h"

#include "des_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief  Imprime la forma de uso del programa.
 * @param  program  Nombre del programa.
 * @param  flags    Opciones extra que admite (OPT_OUTPUT, OPT_CHUNK).
 */
static void print_usage(const char *program, int flags)
{
    fprintf(stderr,
            "Uso: %s -f <archivo.txt> -k <llave> [-p \"frase clave\"] [-b bits] [-t segundos]%s%s\n"
            "\n"
            "  -f  archivo .txt con el texto plano a cifrar\n"
            "  -k  llave privada: número (decimal o 0x...) o preset:\n"
            "        a = 123456          b = 2^bits/4 - 1      c = 2^bits/4\n"
            "        facil = 2^bits/2+1  media = 2^bits/2 + 2^bits/8\n"
            "        dificil = ceil(2^bits/7 + 2^bits/13)\n"
            "  -p  frase clave que debe aparecer en el texto (por defecto \"%s\")\n"
            "  -b  bits del espacio de llaves, %d..%d (por defecto %d)\n"
            "  -t  límite de tiempo de la búsqueda en segundos, 0..%d (0 = sin límite)\n",
            program, (flags & OPT_CHUNK) ? " [-c bits]" : "",
            (flags & OPT_OUTPUT) ? " [-o salida.bin]" : "",
            DEFAULT_PHRASE, MIN_KEY_BITS, DES_KEY_BITS, DES_KEY_BITS, MAX_TIME_LIMIT_S);
    if (flags & OPT_CHUNK) {
        fprintf(stderr,
                "  -c  chunk de 2^bits llaves, %d..%d y menor que -b (por defecto %d);\n"
                "      solo ciclico y maestro_trabajador\n",
                MIN_CHUNK_BITS, MAX_CHUNK_BITS, DEFAULT_CHUNK_BITS);
    }
    if (flags & OPT_OUTPUT) {
        fprintf(stderr, "  -o  guarda el texto cifrado en un archivo binario\n");
    }
    fprintf(stderr, "  -h  muestra esta ayuda\n");
}

/**
 * @brief  Convierte texto a entero sin signo de 64 bits, rechazando basura.
 * @param  text   Texto a convertir (decimal, o hexadecimal con prefijo 0x).
 * @param  value  Salida: el número.
 * @return 0 si la conversión fue completa y sin desbordamiento, -1 si no.
 */
static int parse_u64(const char *text, uint64_t *value)
{
    char *end = NULL;

    if (text == NULL || *text == '\0' || *text == '-') {
        return -1;
    }
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text) {
        return -1;
    }
    /* Se acepta la "L" final para poder copiar las llaves del enunciado tal cual. */
    if (*end == 'L' || *end == 'l') {
        ++end;
    }
    if (*end != '\0') {
        return -1;
    }
    *value = (uint64_t)parsed;
    return 0;
}

int resolve_key(const char *text, int key_bits, uint64_t *key_out)
{
    const uint64_t space = (uint64_t)1 << key_bits;
    uint64_t key;

    if (strcmp(text, "a") == 0) {
        key = 123456ULL;
    } else if (strcmp(text, "b") == 0) {
        key = space / 4 - 1;
    } else if (strcmp(text, "c") == 0) {
        key = space / 4;
    } else if (strcmp(text, "facil") == 0) {
        key = space / 2 + 1;
    } else if (strcmp(text, "media") == 0) {
        key = space / 2 + space / 8;
    } else if (strcmp(text, "dificil") == 0) {
        /* N/7 + N/13 = 20N/91; redondeado hacia arriba. 20 * 2^56 cabe en 64 bits. */
        key = (20 * space + 90) / 91;
    } else if (parse_u64(text, &key) != 0) {
        fprintf(stderr, "Error: la llave \"%s\" no es un número válido ni un preset.\n", text);
        return -1;
    }

    if (key >= space) {
        fprintf(stderr, "Error: la llave %llu no cabe en un espacio de 2^%d llaves.\n",
                (unsigned long long)key, key_bits);
        return -1;
    }
    *key_out = key;
    return 0;
}

int parse_options(int argc, char **argv, const char *program,
                  int flags, program_options_t *opts)
{
    /* Las opciones extra solo entran al optstring si el programa las admite. */
    char optstring[32] = "f:k:p:b:t:h";
    if (flags & OPT_OUTPUT) {
        strcat(optstring, "o:");
    }
    if (flags & OPT_CHUNK) {
        strcat(optstring, "c:");
    }
    int option;

    opts->text_path   = NULL;
    opts->key_text    = NULL;
    opts->key         = 0;
    opts->phrase      = DEFAULT_PHRASE;
    opts->key_bits    = DES_KEY_BITS;
    opts->time_limit_s = 0;
    opts->chunk_bits  = DEFAULT_CHUNK_BITS;
    opts->output_path = NULL;

    opterr = 0;  /* Los mensajes de error los damos nosotros, en español. */
    while ((option = getopt(argc, argv, optstring)) != -1) {
        switch (option) {
        case 'f': opts->text_path   = optarg; break;
        case 'k': opts->key_text    = optarg; break;
        case 'p': opts->phrase      = optarg; break;
        case 'o': opts->output_path = optarg; break;
        case 'b': {
            uint64_t bits;
            if (parse_u64(optarg, &bits) != 0 || bits < MIN_KEY_BITS || bits > DES_KEY_BITS) {
                fprintf(stderr, "Error: -b debe estar entre %d y %d (se recibió \"%s\").\n",
                        MIN_KEY_BITS, DES_KEY_BITS, optarg);
                return OPTIONS_ERROR;
            }
            opts->key_bits = (int)bits;
            break;
        }
        case 't': {
            uint64_t seconds;
            if (parse_u64(optarg, &seconds) != 0 || seconds > MAX_TIME_LIMIT_S) {
                fprintf(stderr, "Error: -t debe estar entre 0 y %d segundos (se recibió \"%s\").\n",
                        MAX_TIME_LIMIT_S, optarg);
                return OPTIONS_ERROR;
            }
            opts->time_limit_s = (int)seconds;
            break;
        }
        case 'c': {
            uint64_t bits;
            if (parse_u64(optarg, &bits) != 0 || bits < MIN_CHUNK_BITS || bits > MAX_CHUNK_BITS) {
                fprintf(stderr, "Error: -c debe estar entre %d y %d (se recibió \"%s\").\n",
                        MIN_CHUNK_BITS, MAX_CHUNK_BITS, optarg);
                return OPTIONS_ERROR;
            }
            opts->chunk_bits = (int)bits;
            break;
        }
        case 'h':
            print_usage(program, flags);
            return OPTIONS_HELP;
        default:
            fprintf(stderr, "Error: opción -%c desconocida o sin valor.\n\n", optopt);
            print_usage(program, flags);
            return OPTIONS_ERROR;
        }
    }

    if (opts->text_path == NULL || opts->key_text == NULL) {
        fprintf(stderr, "Error: -f y -k son obligatorios.\n\n");
        print_usage(program, flags);
        return OPTIONS_ERROR;
    }
    /* Con un solo chunk (o ninguno) no hay nada que repartir. */
    if ((flags & OPT_CHUNK) && opts->chunk_bits >= opts->key_bits) {
        fprintf(stderr, "Error: el chunk (-c %d) debe ser menor que el espacio (-b %d).\n",
                opts->chunk_bits, opts->key_bits);
        return OPTIONS_ERROR;
    }
    if (opts->phrase[0] == '\0') {
        fprintf(stderr, "Error: la frase clave (-p) no puede estar vacía.\n");
        return OPTIONS_ERROR;
    }
    if (strlen(opts->phrase) > MAX_PHRASE_BYTES) {
        fprintf(stderr, "Error: la frase clave supera %d bytes.\n", MAX_PHRASE_BYTES);
        return OPTIONS_ERROR;
    }
    if (resolve_key(opts->key_text, opts->key_bits, &opts->key) != 0) {
        return OPTIONS_ERROR;
    }
    return OPTIONS_OK;
}
