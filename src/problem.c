/**
 * @file    problem.c
 * @brief   Carga del texto, cifrado inicial y reporte de resultados (ver problem.h).
 */
#include "problem.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "des_utils.h"

/**
 * @brief  Lee un archivo de texto completo en memoria.
 * @param  path        Ruta del archivo.
 * @param  length_out  Salida: bytes leídos (sin los saltos de línea finales).
 * @return Buffer con el texto terminado en '\0' (liberar con free), o NULL si falla.
 */
static char *read_text_file(const char *path, size_t *length_out)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Error: no se pudo abrir \"%s\": %s\n", path, strerror(errno));
        return NULL;
    }

    char *text = malloc(MAX_TEXT_BYTES + 1);
    if (text == NULL) {
        fprintf(stderr, "Error: sin memoria para leer \"%s\".\n", path);
        fclose(file);
        return NULL;
    }

    size_t length = fread(text, 1, MAX_TEXT_BYTES + 1, file);
    int read_failed = ferror(file);
    fclose(file);

    if (read_failed) {
        fprintf(stderr, "Error: falló la lectura de \"%s\".\n", path);
        free(text);
        return NULL;
    }
    if (length > MAX_TEXT_BYTES) {
        fprintf(stderr, "Error: \"%s\" supera el máximo de %d bytes.\n", path, MAX_TEXT_BYTES);
        free(text);
        return NULL;
    }
    /* Se quitan los saltos de línea finales que dejan los editores. */
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        --length;
    }
    if (length == 0) {
        fprintf(stderr, "Error: \"%s\" está vacío.\n", path);
        free(text);
        return NULL;
    }
    if (memchr(text, '\0', length) != NULL) {
        fprintf(stderr, "Error: \"%s\" no es texto (contiene bytes nulos).\n", path);
        free(text);
        return NULL;
    }
    text[length] = '\0';
    *length_out = length;
    return text;
}

int problem_prepare(const program_options_t *opts, crack_problem_t *problem)
{
    memset(problem, 0, sizeof(*problem));
    problem->key_bits  = opts->key_bits;
    problem->key_space = (uint64_t)1 << opts->key_bits;

    problem->plain_text = read_text_file(opts->text_path, &problem->text_length);
    if (problem->plain_text == NULL) {
        return -1;
    }

    /*
     * Si la frase no está en el texto, ninguna llave la produciría y la
     * búsqueda recorrería todo el espacio para nada: se rechaza de entrada.
     */
    if (strstr(problem->plain_text, opts->phrase) == NULL) {
        fprintf(stderr, "Error: la frase clave \"%s\" no aparece en el texto de \"%s\".\n",
                opts->phrase, opts->text_path);
        problem_release(problem);
        return -1;
    }

    problem->phrase = strdup(opts->phrase);
    problem->cipher_length = padded_length(problem->text_length);
    /* calloc: el relleno hasta el múltiplo de 8 queda en ceros. */
    problem->cipher = calloc(problem->cipher_length, 1);
    if (problem->phrase == NULL || problem->cipher == NULL) {
        fprintf(stderr, "Error: sin memoria para preparar el cifrado.\n");
        problem_release(problem);
        return -1;
    }

    memcpy(problem->cipher, problem->plain_text, problem->text_length);
    encrypt_des(opts->key, problem->cipher, problem->cipher_length);
    return 0;
}

void problem_release(crack_problem_t *problem)
{
    free(problem->plain_text);
    free(problem->cipher);
    free(problem->phrase);
    problem->plain_text = NULL;
    problem->cipher     = NULL;
    problem->phrase     = NULL;
}

void problem_print_header(const char *program, const program_options_t *opts,
                          const crack_problem_t *problem, int processes)
{
    printf("=== %s ===\n", program);
    printf("Archivo cifrado  : %s (%zu bytes, %zu con relleno)\n",
           opts->text_path, problem->text_length, problem->cipher_length);
    printf("Frase clave      : \"%s\"\n", problem->phrase);
    printf("Llave privada    : %llu (%s)\n", (unsigned long long)opts->key, opts->key_text);
    printf("Espacio de llaves: 2^%d = %llu\n", problem->key_bits,
           (unsigned long long)problem->key_space);
    printf("Procesos         : %d\n", processes);
    fflush(stdout);
}

void problem_print_result(const char *program, const program_options_t *opts,
                          const crack_problem_t *problem, int processes,
                          uint64_t found_key, int finder_rank,
                          uint64_t iterations, double elapsed_s)
{
    unsigned char *plain = malloc(problem->cipher_length + 1);
    if (plain != NULL) {
        memcpy(plain, problem->cipher, problem->cipher_length);
        plain[problem->cipher_length] = '\0';
        decrypt_des(found_key, plain, problem->cipher_length);
    }

    printf("Llave posible    : %llu%s\n", (unsigned long long)found_key,
           found_key == opts->key ? "" : "  (distinta a la real: falso positivo de la frase)");
    printf("Encontrada por   : proceso %d, tras %llu intentos propios\n",
           finder_rank, (unsigned long long)iterations);
    printf("Texto descifrado : \"%s\"\n", plain != NULL ? (char *)plain : "(sin memoria)");
    printf("Tiempo           : %.6f s\n", elapsed_s);
    printf("CSV,%s,%d,%d,%llu,%llu,%d,%llu,%.6f\n", program, processes, problem->key_bits,
           (unsigned long long)opts->key, (unsigned long long)found_key, finder_rank,
           (unsigned long long)iterations, elapsed_s);
    fflush(stdout);
    free(plain);
}

void print_hex(const char *label, const unsigned char *bytes, size_t length)
{
    printf("%s", label);
    for (size_t i = 0; i < length; ++i) {
        printf("%02x%s", bytes[i], (i + 1) % DES_BLOCK_BYTES == 0 && i + 1 < length ? " " : "");
    }
    printf("\n");
}

double wall_seconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}
