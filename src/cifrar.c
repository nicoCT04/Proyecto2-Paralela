/**
 * @file    cifrar.c
 * @brief   Parte B.1: cifra un texto cargado de un .txt con una llave dada por
 *          parámetro y lo descifra de vuelta para demostrar que el ciclo funciona.
 *
 * Ejemplo:  ./bin/cifrar -f data/texto.txt -k 42 -o resultados/cifrado_42.bin
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "des_utils.h"
#include "options.h"
#include "problem.h"

/**
 * @brief  Escribe el texto cifrado en un archivo binario.
 * @param  path    Ruta de salida.
 * @param  bytes   Datos cifrados.
 * @param  length  Bytes a escribir.
 * @return 0 si se escribió completo, -1 si no.
 */
static int write_cipher_file(const char *path, const unsigned char *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        return -1;
    }
    size_t written = fwrite(bytes, 1, length, file);
    if (fclose(file) != 0 || written != length) {
        fprintf(stderr, "Error: no se pudo escribir completo \"%s\".\n", path);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    program_options_t opts;
    crack_problem_t problem;

    int status = parse_options(argc, argv, "cifrar", OPT_OUTPUT, &opts);
    if (status != OPTIONS_OK) {
        return status == OPTIONS_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (problem_prepare(&opts, &problem) != 0) {
        return EXIT_FAILURE;
    }

    printf("=== cifrar ===\n");
    printf("Archivo          : %s\n", opts.text_path);
    printf("Llave privada    : %llu (%s)\n", (unsigned long long)opts.key, opts.key_text);
    printf("Texto original   : \"%s\"\n", problem.plain_text);
    print_hex("Texto cifrado    : ", problem.cipher, problem.cipher_length);

    if (opts.output_path != NULL) {
        if (write_cipher_file(opts.output_path, problem.cipher, problem.cipher_length) != 0) {
            problem_release(&problem);
            return EXIT_FAILURE;
        }
        printf("Guardado en      : %s\n", opts.output_path);
    }

    /* Descifrado de vuelta con la misma llave (DES es simétrico). */
    unsigned char *roundtrip = malloc(problem.cipher_length + 1);
    if (roundtrip == NULL) {
        fprintf(stderr, "Error: sin memoria.\n");
        problem_release(&problem);
        return EXIT_FAILURE;
    }
    memcpy(roundtrip, problem.cipher, problem.cipher_length);
    roundtrip[problem.cipher_length] = '\0';
    decrypt_des(opts.key, roundtrip, problem.cipher_length);
    printf("Descifrado (%llu) : \"%s\"\n", (unsigned long long)opts.key, (char *)roundtrip);

    int matches = memcmp(roundtrip, problem.plain_text, problem.text_length) == 0;
    printf("¿Coincide?       : %s\n", matches ? "sí" : "NO");

    /* Contraejemplo: con la llave vecina sale basura y la frase no aparece. */
    uint64_t wrong_key = opts.key + 1 < problem.key_space ? opts.key + 1 : opts.key - 1;
    memcpy(roundtrip, problem.cipher, problem.cipher_length);
    decrypt_des(wrong_key, roundtrip, problem.cipher_length);
    print_hex("Con llave errónea: ", roundtrip, problem.cipher_length);
    printf("  (llave %llu: la frase clave %s aparece)\n", (unsigned long long)wrong_key,
           try_key(wrong_key, problem.cipher, problem.cipher_length, problem.phrase, roundtrip)
               ? "SÍ" : "no");

    free(roundtrip);
    problem_release(&problem);
    return matches ? EXIT_SUCCESS : EXIT_FAILURE;
}
