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
 *           ./bin/secuencial -f data/texto.txt -k b -t 120
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

    search_result_t result = {0};
    result.finder_rank = -1;
    result.processes   = 1;
    result.threads     = 1;

    double start = wall_seconds();
    double deadline = opts.time_limit_s > 0 ? start + opts.time_limit_s : 0.0;
    for (uint64_t key = 0; key < problem.key_space; ++key) {
        if (deadline > 0.0 && (result.iterations & (POLL_INTERVAL - 1)) == 0
            && wall_seconds() >= deadline) {
            break;
        }
        ++result.iterations;
        if (try_key(key, problem.cipher, problem.cipher_length, problem.phrase, workspace)) {
            result.found_key   = key;
            result.finder_rank = 0;
            result.found       = 1;
            break;
        }
    }
    result.elapsed_s  = wall_seconds() - start;
    result.total_keys = result.iterations;

    if (result.found || deadline > 0.0) {
        problem_print_result("secuencial", &opts, &problem, &result);
    } else {
        /* No debería pasar: la llave real siempre está dentro del espacio. */
        printf("No se encontró la llave en 2^%d intentos (%.6f s).\n", problem.key_bits,
               result.elapsed_s);
    }

    free(workspace);
    problem_release(&problem);
    return result.found || deadline > 0.0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
