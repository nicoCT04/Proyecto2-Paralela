# Anexo 1: Catálogo de funciones y librerías

Todas las funciones del proyecto, incluidas las `static`, agrupadas por archivo, con sus entradas,
salidas y una descripción. Al final están las funciones de librería que se usaron: OpenSSL, Open
MPI, la libc, OpenMP y los módulos de Python.

## `des_utils.c`: DES sobre OpenSSL

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `build_des_key` | `uint64_t key56`; `unsigned char out[8]` | `out`: 8 bytes con 7 bits de llave y 1 de paridad | Reparte los 56 bits en grupos de 7 (bits 1 a 7 de cada byte) y completa la paridad impar con `DES_set_odd_parity`, igual que el `decrypt()` original |
| `run_des` (`static`) | `uint64_t key`; `unsigned char *buffer`; `size_t length` (múltiplo de 8); `int direction` | `buffer` transformado en el mismo lugar | Arma la llave, genera las subllaves con `DES_set_key_unchecked` (sin rechazar llaves débiles) y aplica `DES_ecb_encrypt` bloque a bloque |
| `encrypt_des`, `decrypt_des` | `uint64_t key`; `unsigned char *buffer`; `size_t length` | `buffer` cifrado o descifrado | Llaman a `run_des` con `DES_ENCRYPT` o `DES_DECRYPT`; reemplazan a `encrypt` y `decrypt` del `bruteforce.c` |
| `try_key` | `uint64_t key`; `const unsigned char *cipher`; `size_t length`; `const char *phrase`; `unsigned char *workspace` (`length + 1` bytes) | `int`: 1 si el descifrado contiene la frase | Copia el cifrado al `workspace` con `memcpy`, lo descifra con la llave candidata y busca la frase con `strstr` |
| `padded_length` | `size_t length` | `size_t` | Redondea al siguiente múltiplo de 8 (relleno de ceros hasta completar bloques DES) |

## `options.c`: Parámetros

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `print_usage` (`static`) | `const char *program`; `int flags` | ayuda en stderr | Forma de uso; `-c`, `-o` y `-v` solo aparecen en los programas que las admiten |
| `parse_u64` (`static`) | `const char *text`; `uint64_t *value` | `int`: 0 si la conversión fue completa; `*value` | Envuelve `strtoull`; acepta decimal, `0x` y una `L` final, y rechaza basura, signo o desbordamiento |
| `resolve_key` | `const char *text`; `int key_bits`; `uint64_t *key_out` | `int`: 0 si es válida; `*key_out` | Traduce los presets `a`, `b`, `c`, `facil`, `media` y `dificil` (escalados con N = 2^bits) o convierte el número, y valida que sea menor que N |
| `parse_options` | `argc`, `argv`; `const char *program`; `int flags` (`OPT_OUTPUT`, `OPT_CHUNK`, `OPT_PROGRESS`); `program_options_t *opts` | `OPTIONS_OK`, `OPTIONS_HELP` u `OPTIONS_ERROR`; `*opts` | Lee las opciones con `getopt` y valida los rangos de `-b`, `-t` y `-c`, que `-c` sea menor que `-b`, la frase y la llave; `-v` activa el reporte de avance |

## `problem.c`: Problema y reporte

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `read_text_file` (`static`) | `const char *path`; `size_t *length_out` | texto terminado en `'\0'` o `NULL`; bytes útiles | Lee el archivo completo, hasta 64 KiB; quita saltos de línea finales y rechaza archivos vacíos, grandes o con bytes nulos |
| `problem_prepare` | `const program_options_t *opts`; `crack_problem_t *problem` | `int`: 0 si todo salió bien; `*problem` | Carga el texto, verifica que contenga la frase clave y lo cifra con la llave privada |
| `problem_release` | `crack_problem_t *problem` | punteros liberados y en `NULL` | Libera texto, cifrado y frase; se puede llamar más de una vez |
| `problem_print_header` | programa, `opts`, `problem`, `int processes` | encabezado en stdout | Archivo, frase, llave y tamaño del espacio antes de buscar |
| `problem_print_result` | programa, `opts`, `problem`, `const search_result_t *result` | reporte y línea `CSV,` de 11 columnas | Llave hallada, quién la halló y tras cuántos intentos, texto descifrado, llaves probadas, ritmo y tiempo |
| `print_hex` | `const char *label`; bytes; `size_t length` | hexadecimal en stdout | Imprime un buffer en grupos de 8 bytes |
| `wall_seconds` | — | `double` | Reloj de pared monotónico (`clock_gettime(CLOCK_MONOTONIC)`) |
| `print_progress` | `int worker`; `uint64_t iterations`; `double elapsed_s` | línea `PROGRESS,` | Avance de un trabajador para el visualizador (solo con `-v`) |
| `print_chunk` | `int worker`; `uint64_t first_key`; `double elapsed_s` | línea `CHUNK,` | Chunk que empieza un trabajador del maestro-trabajador (solo con `-v`) |

## `cifrar.c` y `secuencial.c`: Partes B.1 y A.3

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `write_cipher_file` (`static`) | `const char *path`; bytes; `size_t length` | `int`: 0 si se escribió completo | Guarda el cifrado en binario y revisa `fwrite` y `fclose` |
| `main` (cifrar) | `-f`, `-k`, `-p`, `-b`, `-o` | `EXIT_SUCCESS` si el descifrado coincide; texto, cifrado en hexadecimal y contraejemplo | Cifra el `.txt`, lo descifra de vuelta, lo compara con el original y muestra qué sale con la llave vecina |
| `main` (secuencial) | `-f`, `-k`, `-p`, `-b`, `-t`, `-v` | `EXIT_SUCCESS` si la halló o si se usó `-t`; reporte y CSV | Recorre las llaves 0, 1, 2, … hasta que `try_key` acierta o vence el límite, que revisa cada 4096 llaves |

## `search_mpi.c`: Módulo común MPI

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `validate_processes` (`static`) | `int size`; `int min_processes`; `problem` | `int`: 0 si sirve | El proceso 0 rechaza menos procesos que el mínimo (el maestro-trabajador pide 2) o más procesos que llaves |
| `share_problem` (`static`) | `mpi_search_t *search`; `int status` | estado del proceso 0, igual en todos | Difunde con `MPI_Bcast` un encabezado de 7 enteros (estado, bits, longitudes, límite, chunk y `-v`) y luego el cifrado y la frase; si el estado no es «listo», todos salen sin quedar esperando |
| `mpi_search_setup` | `argc`, `argv`; programa; `int option_flags`; `int min_processes`; `search` | `SEARCH_READY`, `SEARCH_HELP` o `SEARCH_ERROR`; `*search` | Inicia MPI si no lo está (`MPI_Initialized`); el proceso 0 lee parámetros y archivo y valida los procesos; se reparte el problema y cada proceso reserva su `workspace` |
| `mpi_search_release` | `search` | memoria liberada y MPI cerrado | Libera el `workspace` y el problema y llama a `MPI_Finalize` |
| `mpi_search_begin` | — | `double`: `MPI_Wtime` tras la barrera | Sincroniza con `MPI_Barrier` y marca el inicio de la búsqueda |
| `notify_all` (`static`) | `const uint64_t message[3]`; `int size` | un `MPI_Send` a cada proceso | Envía el aviso a todos, incluido uno mismo, para que todos los `MPI_Irecv` se completen |
| `announce_timeout` (`static`) | `found_listener_t *listener`; `int size` | `timeout_sent` en 1 | Avisa con la llave centinela `UINT64_MAX` que venció el límite |
| `found_listener_start` | `listener`; `search` | antena puesta, reloj y plazo | Pone el `MPI_Irecv` antes de buscar, sincroniza y arranca el reloj; el proceso 0 fija el plazo |
| `found_listener_poll` | `listener`; `search` | `int`: 1 si ya llegó un aviso | Revisa la antena con `MPI_Test`; el proceso 0 además avisa si venció el plazo |
| `found_listener_announce` | `search`; `uint64_t key`; `uint64_t iterations` | aviso enviado | Arma {llave, rango, iteraciones} y lo envía con `notify_all` |
| `found_listener_progress` | `listener`; `search`; `int worker`; `uint64_t iterations` | línea `PROGRESS,` | Reporta el avance del proceso si se pidió `-v` |
| `found_listener_try_key` | `listener`; `search`; `uint64_t key`; `uint64_t *iterations` | `int`: 1 si hay que parar; contador incrementado | Un paso del bucle estático: sondea cada 4096 llaves (y reporta avance con `-v`), prueba la llave y, si acierta, avisa a todos |
| `found_listener_finish` | `listener`; `search`; `search_result_t *result` | `*result`: hallazgo, llave, proceso, iteraciones y tiempo | El proceso 0 vigila el plazo con pausas de 1 ms; luego todos cierran la antena con `MPI_Wait` y toman el tiempo |
| `mpi_search_report` | `search`; programa; `result`; `uint64_t own_iterations` | reporte y CSV del proceso 0 | Suma las llaves de todos con `MPI_Reduce` y el proceso 0 llama a `problem_print_result` |

## Programas MPI e híbrido

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `main` (naive, intercalado, ciclico) | `-f`, `-k`, `-p`, `-b`, `-t`, `-v` (y `-c` en ciclico) | `EXIT_SUCCESS` o `EXIT_FAILURE`; reporte y CSV | `mpi_search_setup` → `found_listener_start` → bucle con `found_listener_try_key` → `found_listener_finish` → `mpi_search_report`. Solo cambia el recorrido: bloque `[r·N/p, (r+1)·N/p)`, llaves `r, r+p, …` o chunks `r, r+p, …` de 2^c llaves |
| `run_master` (`static`) | `search`; `search_result_t *result` | `*result` con hallazgo, proceso, iteraciones y T_p | Atiende pedidos con `MPI_Recv(ANY_SOURCE)` y responde a cada uno un chunk (`TAG_WORK`) o `TAG_STOP`; responde STOP desde el hallazgo (ahí toma T_p), el plazo o el fin de los chunks, y termina al recibir el mensaje final de cada trabajador |
| `search_chunk` (`static`) | `search`; `const uint64_t work[2]`; `double start`; `uint64_t *iterations`; `uint64_t *found_key` | `int`: 1 si la halló | Prueba en orden las llaves de un chunk; con `-v` reporta el chunk y el avance |
| `run_worker` (`static`) | `search` | `uint64_t`: llaves probadas | Pide el chunk siguiente por adelantado (`MPI_Send` + `MPI_Irecv`), prueba el actual y espera el próximo con `MPI_Wait`; termina con FOUND o DONE |
| `main` (maestro_trabajador) | `-f`, `-k`, `-p`, `-b`, `-t`, `-c`, `-v` | `EXIT_FAILURE` con `-np 1`; reporte y CSV | El proceso 0 corre `run_master` y los demás `run_worker`; el maestro aporta 0 llaves |
| `init_threaded_mpi` (`static`) | `argc`, `argv` | `int`: 0 si MPI da `MPI_THREAD_FUNNELED` | Inicia MPI con `MPI_Init_thread` y valida el nivel concedido |
| `share_threads` (`static`) | `search` | `int`: hilos por proceso T, o −1 | El proceso 0 lee T con `omp_get_max_threads`, valida 1 ≤ T ≤ 256 y p·T ≤ N, y lo difunde con `MPI_Bcast` |
| `search_thread` (`static`) | `listener`; `search`; `int threads`; `thread_shared_t *shared` | `uint64_t`: llaves del hilo; hallazgo en `*shared` | El hilo t del proceso r prueba g, g + pT, … con g = r·T + t; cada 4096 llaves revisa la bandera atómica, el hilo 0 sondea la antena y, con `-v`, cada hilo reporta su avance |
| `main` (hibrido) | `-f`, `-k`, `-p`, `-b`, `-t`, `-v`; `OMP_NUM_THREADS` | reporte y CSV con `hilos = T` | Abre la región paralela con `reduction(+)` de las llaves y, fuera de ella, el hilo principal envía el aviso si un hilo halló la llave |

## `scripts/analizar.py`: Análisis de la campaña

Funciones de Python que convierten `resultados/campana_<host>_*.csv` en
`resultados/analisis_<host>.md` y las gráficas de `docs/img/`.

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `Group` (clase) | caso, programa, procesos, hilos, bits, llave real | objeto con listas de tiempos, iteraciones y llaves probadas | Repeticiones de un mismo caso; `time`, `finder_iterations` y `keys_tried` dan la mediana y `workers` = p·h |
| `case_rank`, `program_rank` | `str` caso o programa | `int` | Orden fijo de llaves y programas en tablas y gráficas |
| `host_from_paths` | `list[str]` rutas | `str` host | Saca el host del nombre `campana_<host>_<fecha>.csv` |
| `load_groups` | `list[str]` rutas | grupos válidos, todos los grupos, `int` filas | Lee los CSV, agrupa y aparta las corridas con `llave_hallada != llave_real` |
| `sequential_times` | grupos | `dict` (caso, bits) → T_s | Mediana del `secuencial` por llave y bits |
| `metrics` | `Group`, `float` T_s | `dict` S, E, C, F, S_iter, S_rend | Las fórmulas de clase con w = p·h |
| `amdahl_fp` | `float` S, `int` p | `float` f_p | f_p = (1 − 1/S)/(1 − 1/p) |
| `amdahl_speedup` | `float` f_p, `float` p | `float` S | 1/(f_s + f_p/p) |
| `gustafson_speedup` | `float` f_s, `int` p | `float` S | p − f_s(p − 1) |
| `ideal_speedup` | `Group` | `int` | p·h, o p − 1 en el maestro-trabajador |
| `note` | `Group`, métricas, lista de alertas | `str` nota | Regla del superlineal: «aparente (búsqueda)» en el naive; `REVISAR` si otro programa da S > w o cualquiera da S_rend > w (tolerancia 2 %) |
| `fmt` | `float`, dígitos | `str` | Número legible, en notación científica si es enorme o diminuto |
| `time_cell`, `label` | `Group` | `str` | Celda «mediana (mín–máx)» y nombre del programa con sus hilos |
| `main_table` | líneas, grupos, T_s, alertas | líneas de Markdown | Tabla de T_p, S, E, C, F, S_iter, S_rend y nota |
| `amdahl_section` | líneas, grupos, T_s, p principal, llave del barrido | `dict` programa → f_p | f_p, f_s, techo, Gustafson y bits extra; predicción de Amdahl frente al barrido |
| `style_axes` | eje de matplotlib | — | Rejilla y ejes discretos |
| `plot_speedup_by_key` | matplotlib, grupos, T_s, p, ruta | PNG | Barras de S por programa y llave con la línea S = p; el naive aparente se recorta y se rotula |
| `plot_sweep` | matplotlib, grupos, T_s, f_p, llave, ruta | PNG | S contra p con la ideal y la curva de Amdahl |
| `plot_hybrid` | matplotlib, grupos, T_s, ruta | PNG | Tiempo y speedup del híbrido contra los hilos |
| `plot_all` | grupos, T_s, f_p, p, llave, host, carpeta | `list[str]` rutas | Genera las gráficas si hay matplotlib; si no, avisa y sigue |
| `analyze` | rutas, salida, carpeta de imágenes | `list[str]` alertas | Arma el `.md` completo e imprime las alertas |
| `parse_args`, `main` | línea de comandos | código de salida | 1 si hubo alguna alerta `REVISAR` |

## `scripts/key_position_plots.py`: Figuras 12 y 13

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `naive_tries` | `int` llave k | `int` | Intentos del proceso del naive que tiene k: k − r·(N/p) + 1 |
| `naive_wins` | `int` llave k | `bool` | Si el naive necesita menos intentos que el intercalado (⌊k/p⌋ + 1) |
| `key_samples`, `zones` | — | lista de llaves; tramos (inicio, fin, gana el naive) | Puntos de la curva, incluidos los saltos de bloque, y zonas de fondo de la figura 12 |
| `style_axes` | eje de matplotlib | — | Mismo estilo que `analizar.py` |
| `plot_tries` | ruta del PNG | `docs/img/intentos_por_llave.png` | Intentos de cada programa según la posición de la llave, con las llaves 60 000 000 y 16 000 000 marcadas |
| `load_campaign` | — | `dict` (llave, programa) → tiempo | Lee la campaña de 16 llaves al azar (híbrido con 2 hilos) |
| `plot_measured` | ruta del PNG | `docs/img/speedup_llaves_azar.png` | Speedup medido de cada programa según la posición de la llave |
| `main` | — | código de salida | Genera las dos figuras |

## `visualizer/`: Visualizador web

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `bounded_int` | parámetros, nombre, mínimo, máximo | `int` | Valida que un parámetro sea un entero en rango; si no, lanza `BadRequest` |
| `build_command` | `dict` con programa, llave, bits, procesos, hilos, chunk y límite | comando, entorno y texto | Arma la llamada real (`mpirun ... -v`, con `--bind-to none` y `OMP_NUM_THREADS` en el híbrido); solo admite los seis programas |
| `parse_line` | `str` línea del programa | `dict` | Traduce las líneas `PROGRESS`, `CHUNK` y `CSV` a eventos JSON |
| `stop_process` | proceso | — | Termina el grupo de procesos de `mpirun` |
| `cpu_model` | — | `str` | Modelo de CPU para el encabezado de la página |
| `Handler` (clase) | peticiones HTTP | página, `/api/info`, `/api/run`, `/api/stop` | Solo atiende a `127.0.0.1`; corre un programa a la vez y transmite su avance línea por línea |
| `index.html` | eventos del servidor | rectángulo, lupa, carrera y tabla | Dibuja el espacio de llaves con un color por proceso, repite las corridas con el mismo reloj y compara tiempos y speedups |

## Librerías

### OpenSSL (`<openssl/des.h>`, `-lcrypto`)

| Función o tipo | Uso en el proyecto |
|---|---|
| `DES_cblock` | Bloque o llave de 8 bytes |
| `DES_key_schedule` | Subllaves de las 16 rondas |
| `DES_set_odd_parity` | Completa la paridad impar de cada byte de la llave |
| `DES_set_key_unchecked` | Genera el *key schedule* sin rechazar llaves débiles |
| `DES_ecb_encrypt` | Cifra o descifra un bloque de 8 bytes en modo ECB |

### Open MPI (`<mpi.h>`)

| Primitiva | Uso en el proyecto |
|---|---|
| `MPI_Init`, `MPI_Finalize` | Abren y cierran MPI (`mpi_search_setup` y `mpi_search_release`) |
| `MPI_Init_thread` | El híbrido inicia MPI pidiendo `MPI_THREAD_FUNNELED` |
| `MPI_Initialized` | `mpi_search_setup` no vuelve a iniciar MPI si el híbrido ya lo hizo |
| `MPI_Comm_rank`, `MPI_Comm_size` | Rango propio y número de procesos |
| `MPI_Bcast` | Reparte el encabezado, el cifrado y la frase desde el proceso 0 |
| `MPI_Barrier` | Sincroniza el inicio de la búsqueda |
| `MPI_Wtime` | Reloj de pared para T_p y para el límite de tiempo |
| `MPI_Irecv` | Antena de aviso y pedido adelantado del maestro-trabajador |
| `MPI_Send` | Aviso de llave, pedidos y respuestas del maestro-trabajador |
| `MPI_Recv` | El maestro recibe pedidos; el trabajador recibe chunks y STOP |
| `MPI_Test` | Sondeo de la antena cada 4096 llaves (hace avanzar la recepción) |
| `MPI_Wait` | Cierra la antena y espera el chunk adelantado |
| `MPI_Reduce` | Suma de las llaves probadas por todos |
| `MPI_Abort` | Solo si falta memoria en un proceso |

### libc

| Función | Uso en el proyecto |
|---|---|
| `memcpy` | Copia del cifrado al `workspace` en cada intento |
| `memset`, `memchr` | Inicializar estructuras; detectar bytes nulos en el `.txt` |
| `strstr` | Buscar la frase clave en el texto descifrado |
| `strlen`, `strcmp`, `strcat`, `strdup` | Frase, presets de llave, optstring y copia de la frase |
| `getopt` | Leer las opciones de la línea de comandos |
| `strtoull` | Convertir números de 64 bits (`parse_u64`) |
| `malloc`, `calloc`, `free` | Memoria del problema y del `workspace` (`calloc` deja el relleno en ceros) |
| `fopen`, `fread`, `fwrite`, `fclose`, `ferror` | Leer el `.txt` y guardar el cifrado |
| `printf`, `fprintf`, `snprintf`, `fflush`, `perror`, `strerror` | Reportes y mensajes de error |
| `clock_gettime` | Reloj monotónico del secuencial |
| `nanosleep` | Pausa de 1 ms del proceso 0 mientras vigila el límite |

### OpenMP y C11 (híbrido)

| Primitiva | Uso en el proyecto |
|---|---|
| `#pragma omp parallel num_threads(T) reduction(+ : iterations)` | Región paralela con T hilos; suma las llaves probadas de todos los hilos |
| `#pragma omp critical(found_key)` | Guarda llave e iteraciones del primer hilo que la halla |
| `omp_get_max_threads` | El proceso 0 lee T de `OMP_NUM_THREADS` |
| `omp_get_thread_num` | Número de hilo t, para el trabajador global g = r·T + t |
| `atomic_int`, `atomic_init`, `atomic_store`, `atomic_load_explicit` (`<stdatomic.h>`) | Bandera de parada compartida entre los hilos de un proceso |

### Python (análisis)

| Módulo | Uso |
|---|---|
| `csv`, `statistics.median` | Leer la campaña y tomar la mediana de las repeticiones |
| `math.log2` | Bits extra según Gustafson |
| `argparse`, `os`, `re`, `sys` | Línea de comandos, rutas y alertas |
| `matplotlib` (opcional) | Gráficas en `docs/img/` |
