/**
 * @file    secuencial.c
 * @brief   Parte A.3: búsqueda secuencial de la llave por fuerza bruta.
 *
 * Un solo proceso recorre las llaves 0, 1, 2, ... hasta que una descifra un
 * texto que contiene la frase clave. Es la base contra la que se calcula el
 * speedup de las versiones paralelas.
 *
 * Ejemplo:  ./bin/secuencial -f data/texto.txt -k 123456
 *           ./bin/secuencial -f data/texto.txt -k facil -b 24
 */
#include <stdio.h>
#include <stdlib.h>

#include "des_utils.h"
#include "options.h"
#include "problem.h"

int main(int argc, char **argv)
{
    program_options_t opts;
    crack_problem_t problem;

    int status = parse_options(argc, argv, "secuencial", 0, &opts);
    if (status != OPTIONS_OK) {
        return status == OPTIONS_HELP ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (problem_prepare(&opts, &problem) != 0) {
        return EXIT_FAILURE;
    }
    problem_print_header("secuencial", &opts, &problem, 1);

    /* Buffer de trabajo de try_key: se reserva una vez, no en cada intento. */
    unsigned char *workspace = malloc(problem.cipher_length + 1);
    if (workspace == NULL) {
        fprintf(stderr, "Error: sin memoria.\n");
        problem_release(&problem);
        return EXIT_FAILURE;
    }

    uint64_t found_key = 0;
    uint64_t iterations = 0;
    int found = 0;

    double start = wall_seconds();
    for (uint64_t key = 0; key < problem.key_space; ++key) {
        ++iterations;
        if (try_key(key, problem.cipher, problem.cipher_length, problem.phrase, workspace)) {
            found_key = key;
            found = 1;
            break;
        }
    }
    double elapsed = wall_seconds() - start;

    if (found) {
        problem_print_result("secuencial", &opts, &problem, 1, found_key, 0, iterations, elapsed);
    } else {
        /* No debería pasar: la llave real siempre está dentro del espacio. */
        printf("No se encontró la llave en 2^%d intentos (%.6f s).\n", problem.key_bits, elapsed);
    }

    free(workspace);
    problem_release(&problem);
    return found ? EXIT_SUCCESS : EXIT_FAILURE;
}
