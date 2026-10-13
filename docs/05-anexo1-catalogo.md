# Anexo 1 · Catálogo de funciones y librerías

Todas las funciones del proyecto, incluidas las `static`, agrupadas por archivo. Cada una lleva
sus entradas y salidas (nombre, tipo y uso) y una descripción. Al final están las funciones de
librería que usamos: OpenSSL, Open MPI, la libc, OpenMP y los módulos de Python del análisis.

## `des_utils.c` · DES sobre OpenSSL

### `build_des_key`

| Entradas | Tipo | Uso |
|---|---|---|
| `key56` | `uint64_t` | Llave numérica; solo se usan sus 56 bits bajos |
| `out` | `unsigned char[8]` | Buffer donde se escribe la llave DES |

| Salidas | Tipo | Uso |
|---|---|---|
| `out` | `unsigned char[8]` | 8 bytes con 7 bits de llave y 1 de paridad cada uno |

**Descripción.** Reparte los 56 bits en grupos de 7, uno por byte (bits 1..7), y completa la
paridad impar con `DES_set_odd_parity`, igual que el `decrypt()` del `bruteforce.c` original.

### `run_des` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `key` | `uint64_t` | Llave de 56 bits |
| `buffer` | `unsigned char *` | Datos que se cifran o descifran en el mismo lugar |
| `length` | `size_t` | Bytes de `buffer`, múltiplo de 8 |
| `direction` | `int` | `DES_ENCRYPT` o `DES_DECRYPT` |

| Salidas | Tipo | Uso |
|---|---|---|
| `buffer` | `unsigned char *` | Datos ya transformados |

**Descripción.** Arma la llave, genera el *key schedule* con `DES_set_key_unchecked` (sin
rechazar llaves débiles) y aplica `DES_ecb_encrypt` bloque a bloque.

### `encrypt_des` y `decrypt_des`

| Entradas | Tipo | Uso |
|---|---|---|
| `key` | `uint64_t` | Llave de 56 bits |
| `buffer` | `unsigned char *` | Texto plano (cifrar) o cifrado (descifrar) |
| `length` | `size_t` | Bytes de `buffer`, múltiplo de 8 |

| Salidas | Tipo | Uso |
|---|---|---|
| `buffer` | `unsigned char *` | Resultado, en el mismo buffer |

**Descripción.** Cifran o descifran con DES en modo ECB llamando a `run_des`. Reemplazan a
`encrypt` y `decrypt` del `bruteforce.c`.

### `try_key`

| Entradas | Tipo | Uso |
|---|---|---|
| `key` | `uint64_t` | Llave candidata |
| `cipher` | `const unsigned char *` | Texto cifrado original (no se modifica) |
| `length` | `size_t` | Bytes del cifrado |
| `phrase` | `const char *` | Frase clave conocida |
| `workspace` | `unsigned char *` | Buffer de `length + 1` bytes reservado una sola vez |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 1 si el descifrado contiene la frase, 0 si no |

**Descripción.** Copia el cifrado al `workspace` con `memcpy`, lo descifra con la llave
candidata y busca la frase con `strstr`.

### `padded_length`

| Entradas | Tipo | Uso |
|---|---|---|
| `length` | `size_t` | Longitud original |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `size_t` | Longitud redondeada al siguiente múltiplo de 8 |

**Descripción.** Calcula cuántos bytes ocupa el texto con relleno de ceros hasta completar
bloques DES.

## `options.c` · Parámetros

### `print_usage` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `program` | `const char *` | Nombre del programa |
| `flags` | `int` | Opciones extra que admite (`OPT_OUTPUT`, `OPT_CHUNK`) |

| Salidas | Tipo | Uso |
|---|---|---|
| (stderr) | texto | Ayuda del programa |

**Descripción.** Imprime la forma de uso. `-c` y `-o` solo aparecen en los programas que las
admiten.

### `parse_u64` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `text` | `const char *` | Número en decimal o en hexadecimal con `0x`; acepta una `L` final |
| `value` | `uint64_t *` | Destino del número |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 0 si la conversión fue completa, -1 si hay basura, signo o desbordamiento |
| `*value` | `uint64_t` | El número convertido |

**Descripción.** Envuelve `strtoull` y rechaza todo lo que no sea un número completo.

### `resolve_key`

| Entradas | Tipo | Uso |
|---|---|---|
| `text` | `const char *` | Llave escrita en `-k`: número o preset |
| `key_bits` | `int` | Bits del espacio de búsqueda |
| `key_out` | `uint64_t *` | Destino de la llave |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 0 si es válida y cabe en 2^bits, -1 si no |
| `*key_out` | `uint64_t` | La llave resuelta |

**Descripción.** Traduce los presets `a`, `b`, `c`, `facil`, `media` y `dificil` (escalados con
N = 2^bits) o convierte el número, y valida que la llave sea menor que N.

### `parse_options`

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int`, `char **` | Argumentos del programa |
| `program` | `const char *` | Nombre para la ayuda |
| `flags` | `int` | Opciones extra admitidas (`OPT_OUTPUT`, `OPT_CHUNK`) |
| `opts` | `program_options_t *` | Destino de los parámetros |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | `OPTIONS_OK`, `OPTIONS_HELP` u `OPTIONS_ERROR` |
| `*opts` | `program_options_t` | Archivo, llave, frase, bits, límite de tiempo, chunk y salida |

**Descripción.** Lee las opciones con `getopt` y las valida: rangos de `-b`, `-t` y `-c`, que
`-c` sea menor que `-b`, que la frase no esté vacía y que la llave sea válida. Los mensajes de
error salen en español.

## `problem.c` · Problema y reporte

### `read_text_file` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `path` | `const char *` | Ruta del `.txt` |
| `length_out` | `size_t *` | Destino de la longitud |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `char *` | Texto terminado en `'\0'` (liberar con `free`), o `NULL` si falla |
| `*length_out` | `size_t` | Bytes útiles, sin los saltos de línea finales |

**Descripción.** Lee el archivo completo, hasta 64 KiB. Rechaza archivos vacíos, demasiado
grandes o con bytes nulos.

### `problem_prepare`

| Entradas | Tipo | Uso |
|---|---|---|
| `opts` | `const program_options_t *` | Parámetros validados |
| `problem` | `crack_problem_t *` | Destino del problema |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 0 si todo salió bien, -1 si no |
| `*problem` | `crack_problem_t` | Texto, cifrado con relleno, frase y tamaño del espacio |

**Descripción.** Carga el texto, verifica que contenga la frase clave y lo cifra con la llave
privada.

### `problem_release`

| Entradas | Tipo | Uso |
|---|---|---|
| `problem` | `crack_problem_t *` | Problema, completo o a medio construir |

| Salidas | Tipo | Uso |
|---|---|---|
| `*problem` | `crack_problem_t` | Punteros liberados y en `NULL` |

**Descripción.** Libera el texto, el cifrado y la frase. Se puede llamar más de una vez.

### `problem_print_header`

| Entradas | Tipo | Uso |
|---|---|---|
| `program` | `const char *` | Nombre del programa |
| `opts` | `const program_options_t *` | Parámetros |
| `problem` | `const crack_problem_t *` | Problema preparado |
| `processes` | `int` | Número de procesos |

| Salidas | Tipo | Uso |
|---|---|---|
| (stdout) | texto | Encabezado de la corrida |

**Descripción.** Imprime el archivo, la frase, la llave y el tamaño del espacio antes de buscar.

### `problem_print_result`

| Entradas | Tipo | Uso |
|---|---|---|
| `program` | `const char *` | Nombre del programa |
| `opts` | `const program_options_t *` | Llave real y límite de tiempo |
| `problem` | `const crack_problem_t *` | Para descifrar y mostrar el texto |
| `result` | `const search_result_t *` | Resultado de la búsqueda |

| Salidas | Tipo | Uso |
|---|---|---|
| (stdout) | texto | Reporte y línea `CSV,` de 11 columnas |

**Descripción.** Muestra la llave hallada, quién la halló y en cuántas iteraciones, el texto
descifrado, las llaves probadas, el ritmo y el tiempo, y deja la línea CSV para la bitácora.

### `print_hex`

| Entradas | Tipo | Uso |
|---|---|---|
| `label` | `const char *` | Etiqueta de la línea |
| `bytes` | `const unsigned char *` | Datos |
| `length` | `size_t` | Bytes a imprimir |

| Salidas | Tipo | Uso |
|---|---|---|
| (stdout) | texto | Bytes en hexadecimal, en grupos de 8 |

**Descripción.** Imprime un buffer en hexadecimal, separado por bloques DES.

### `wall_seconds`

| Entradas | Tipo | Uso |
|---|---|---|
| — | — | — |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `double` | Segundos del reloj monotónico |

**Descripción.** Reloj de pared del secuencial (`clock_gettime(CLOCK_MONOTONIC)`).

## `cifrar.c` · Parte B.1

### `write_cipher_file` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `path` | `const char *` | Archivo de salida |
| `bytes` | `const unsigned char *` | Texto cifrado |
| `length` | `size_t` | Bytes a escribir |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 0 si se escribió completo, -1 si no |

**Descripción.** Guarda el cifrado en binario y revisa tanto `fwrite` como `fclose`.

### `main` (cifrar)

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int`, `char **` | `-f`, `-k`, `-p`, `-b`, `-o` |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | `EXIT_SUCCESS` si el descifrado coincide con el original |
| (stdout) | texto | Texto, cifrado en hexadecimal, descifrado y contraejemplo |

**Descripción.** Cifra el `.txt` con la llave dada, lo descifra de vuelta, lo compara con el
original y muestra qué sale con la llave vecina.

## `secuencial.c` · Parte A.3

### `main` (secuencial)

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int`, `char **` | `-f`, `-k`, `-p`, `-b`, `-t` |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | `EXIT_SUCCESS` si la halló o si se usó `-t` |
| (stdout) | texto | Reporte y línea CSV |

**Descripción.** Recorre las llaves 0, 1, 2, … hasta que `try_key` acierta, o hasta que vence
el límite de tiempo, que revisa cada 4096 llaves.

## `search_mpi.c` · Módulo común MPI

### `validate_processes` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `size` | `int` | Número de procesos |
| `min_processes` | `int` | Mínimo que necesita el programa |
| `problem` | `const crack_problem_t *` | Para comparar con 2^bits |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 0 si el número de procesos sirve, -1 si no |

**Descripción.** El proceso 0 rechaza menos procesos que el mínimo (el maestro-trabajador pide
2) o más procesos que llaves.

### `share_problem` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `mpi_search_t *` | Búsqueda; en el proceso 0 ya preparada |
| `status` | `int` | Estado del proceso 0: listo, ayuda o error |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | El estado del proceso 0, igual en todos |
| `*search` | `mpi_search_t` | En los demás procesos: cifrado, frase, bits, límite y chunk |

**Descripción.** Difunde con `MPI_Bcast` un encabezado de 6 enteros (estado, bits, longitudes,
límite de tiempo y chunk) y luego el cifrado y la frase. Si el estado no es «listo», todos
salen sin quedar esperando.

### `mpi_search_setup`

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int *`, `char ***` | Argumentos (se pasan a `MPI_Init`) |
| `program` | `const char *` | Nombre del programa |
| `option_flags` | `int` | 0 u `OPT_CHUNK` |
| `min_processes` | `int` | Mínimo de procesos |
| `search` | `mpi_search_t *` | Destino del estado |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | `SEARCH_READY`, `SEARCH_HELP` o `SEARCH_ERROR` |
| `*search` | `mpi_search_t` | Rango, procesos, parámetros, problema y `workspace` |

**Descripción.** Inicia MPI con `MPI_Init`, salvo que ya esté iniciado (`MPI_Initialized`; el
híbrido lo inicia antes con `MPI_Init_thread`). El proceso 0 lee los parámetros y el archivo y valida el número de
procesos; luego se reparte el problema, el proceso 0 imprime el encabezado y cada proceso
reserva su `workspace`. Si no hay que buscar, cierra MPI antes de volver.

### `mpi_search_release`

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `mpi_search_t *` | Búsqueda |

| Salidas | Tipo | Uso |
|---|---|---|
| — | — | Memoria liberada y MPI cerrado |

**Descripción.** Libera el `workspace` y el problema y llama a `MPI_Finalize`.

### `mpi_search_begin`

| Entradas | Tipo | Uso |
|---|---|---|
| — | — | — |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `double` | `MPI_Wtime` justo después de la barrera |

**Descripción.** Sincroniza a todos con `MPI_Barrier` y marca el inicio de la búsqueda.

### `notify_all` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `message` | `const uint64_t[3]` | {llave, proceso, iteraciones} |
| `size` | `int` | Número de procesos |

| Salidas | Tipo | Uso |
|---|---|---|
| — | — | Un `MPI_Send` a cada proceso |

**Descripción.** Envía el aviso a todos, incluido uno mismo, para que todos los `MPI_Irecv` se
completen.

### `announce_timeout` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `listener` | `found_listener_t *` | Antena del proceso 0 |
| `size` | `int` | Número de procesos |

| Salidas | Tipo | Uso |
|---|---|---|
| `listener->timeout_sent` | `int` | Queda en 1 |

**Descripción.** Avisa a todos con la llave centinela `UINT64_MAX` que venció el límite de
tiempo.

### `found_listener_start`

| Entradas | Tipo | Uso |
|---|---|---|
| `listener` | `found_listener_t *` | Destino de la antena |
| `search` | `const mpi_search_t *` | Para el límite de tiempo |

| Salidas | Tipo | Uso |
|---|---|---|
| `*listener` | `found_listener_t` | `MPI_Irecv` puesto, reloj iniciado y plazo (solo en el proceso 0) |

**Descripción.** Pone la antena antes de buscar, sincroniza y arranca el reloj.

### `found_listener_poll`

| Entradas | Tipo | Uso |
|---|---|---|
| `listener` | `found_listener_t *` | Antena |
| `search` | `const mpi_search_t *` | Número de procesos |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 1 si ya llegó un aviso, 0 si no |

**Descripción.** Revisa la antena con `MPI_Test`; el proceso 0 además manda el aviso de límite
si el plazo ya venció.

### `found_listener_announce`

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `const mpi_search_t *` | Rango y número de procesos |
| `key` | `uint64_t` | Llave hallada |
| `iterations` | `uint64_t` | Llaves que probó este proceso |

| Salidas | Tipo | Uso |
|---|---|---|
| — | — | Aviso enviado a todos |

**Descripción.** Arma {llave, rango, iteraciones} y lo envía con `notify_all`.

### `found_listener_try_key`

| Entradas | Tipo | Uso |
|---|---|---|
| `listener` | `found_listener_t *` | Antena |
| `search` | `const mpi_search_t *` | Cifrado, frase y `workspace` |
| `key` | `uint64_t` | Llave candidata |
| `iterations` | `uint64_t *` | Contador de llaves de este proceso |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 1 si hay que parar (la halló este proceso u otro avisó) |
| `*iterations` | `uint64_t` | Incrementado si se probó la llave |

**Descripción.** Es un paso del bucle de los repartos estáticos: sondea cada 4096 llaves,
prueba la llave y, si acierta, avisa a todos.

### `found_listener_finish`

| Entradas | Tipo | Uso |
|---|---|---|
| `listener` | `found_listener_t *` | Antena |
| `search` | `const mpi_search_t *` | Número de procesos |
| `result` | `search_result_t *` | Destino del resultado |

| Salidas | Tipo | Uso |
|---|---|---|
| `*result` | `search_result_t` | Hallazgo, llave, proceso, iteraciones y tiempo |

**Descripción.** El proceso 0 sigue vigilando el plazo, con pausas de 1 ms, mientras espera.
Luego todos cierran la antena con `MPI_Wait`, toman el tiempo y arman el resultado.

### `mpi_search_report`

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `const mpi_search_t *` | Búsqueda |
| `program` | `const char *` | Nombre del programa |
| `result` | `const search_result_t *` | Resultado (se usa en el proceso 0) |
| `own_iterations` | `uint64_t` | Llaves que probó este proceso |

| Salidas | Tipo | Uso |
|---|---|---|
| (stdout) | texto | Reporte y CSV, impresos por el proceso 0 |

**Descripción.** Suma las llaves de todos con `MPI_Reduce` y el proceso 0 llama a
`problem_print_result`.

## Programas MPI

### `main` (naive, intercalado, ciclico)

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int`, `char **` | `-f`, `-k`, `-p`, `-b`, `-t` (y `-c` en ciclico) |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | `EXIT_SUCCESS`, o `EXIT_FAILURE` si los parámetros son inválidos |
| (stdout) | texto | Reporte y CSV del proceso 0 |

**Descripción.** Los tres siguen el mismo esqueleto: `mpi_search_setup` →
`found_listener_start` → bucle con `found_listener_try_key` → `found_listener_finish` →
`mpi_search_report` → `mpi_search_release`. Solo cambia el recorrido:
- naive: bloque contiguo `[r·N/p, (r+1)·N/p)`;
- intercalado: `r, r+p, r+2p, …`;
- ciclico: chunks `r, r+p, …` de 2^c llaves, cada uno en orden.

### `run_master` (`static`, maestro_trabajador)

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `const mpi_search_t *` | Espacio, chunk, límite y procesos |
| `result` | `search_result_t *` | Destino del resultado |

| Salidas | Tipo | Uso |
|---|---|---|
| `*result` | `search_result_t` | Hallazgo, proceso, iteraciones y T_p |

**Descripción.** Atiende pedidos con `MPI_Recv(ANY_SOURCE)`. A cada pedido (ASK) responde
exactamente un mensaje: un chunk (`TAG_WORK`) o `TAG_STOP`. Responde STOP desde que llega el
hallazgo (y ahí toma T_p), cuando vence el plazo o cuando se acaban los chunks. Los mensajes
finales (FOUND o DONE) no llevan respuesta y descuentan un trabajador activo; termina cuando
recibió el mensaje final de todos.

### `search_chunk` (`static`, maestro_trabajador)

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `const mpi_search_t *` | Cifrado, frase y `workspace` |
| `work` | `const uint64_t[2]` | {primera llave, una más allá de la última} |
| `iterations` | `uint64_t *` | Contador de llaves del trabajador |
| `found_key` | `uint64_t *` | Destino de la llave |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 1 si la halló, 0 si no |

**Descripción.** Prueba en orden todas las llaves de un chunk.

### `run_worker` (`static`, maestro_trabajador)

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `const mpi_search_t *` | Búsqueda |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `uint64_t` | Llaves que probó el trabajador |

**Descripción.** Pide un chunk y, apenas lo recibe, pide el siguiente por adelantado
(`MPI_Send` + `MPI_Irecv`). Prueba el actual y espera el próximo con `MPI_Wait`. Si halla la
llave, envía de inmediato el mensaje final FOUND y recibe (y descarta) la respuesta pendiente;
si recibe STOP sin hallarla, envía el mensaje final DONE.

### `main` (maestro_trabajador)

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int`, `char **` | `-f`, `-k`, `-p`, `-b`, `-t`, `-c` |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | `EXIT_SUCCESS`, o `EXIT_FAILURE` con parámetros inválidos o `-np 1` |
| (stdout) | texto | Reporte y CSV del proceso 0 |

**Descripción.** Prepara la búsqueda con mínimo 2 procesos. El proceso 0 corre `run_master` y
los demás `run_worker`; al final, `mpi_search_report` con 0 llaves aportadas por el maestro.

## `hibrido.c` · MPI + OpenMP

### `init_threaded_mpi` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int *`, `char ***` | Argumentos (se pasan a `MPI_Init_thread`) |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | 0 si MPI da al menos `MPI_THREAD_FUNNELED`, −1 si no (ya avisó y cerró MPI) |

**Descripción.** Inicia MPI con `MPI_Init_thread` pidiendo `MPI_THREAD_FUNNELED` (solo el hilo
principal llama a MPI) y valida el nivel que el MPI instalado concede en `provided`.

### `share_threads` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `search` | `const mpi_search_t *` | Rango, procesos y espacio de llaves |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | Hilos por proceso T, o −1 si no sirven |

**Descripción.** El proceso 0 lee T con `omp_get_max_threads` (es decir, de `OMP_NUM_THREADS`),
valida 1 ≤ T ≤ 256 y que p·T no supere las llaves del espacio, y lo difunde con `MPI_Bcast` para
que todos usen el mismo T aunque el entorno de algún proceso difiera.

### `search_thread` (`static`)

| Entradas | Tipo | Uso |
|---|---|---|
| `listener` | `found_listener_t *` | Antena del proceso; solo la toca el hilo 0 |
| `search` | `const mpi_search_t *` | Problema, rango y procesos |
| `threads` | `int` | Hilos por proceso T |
| `shared` | `thread_shared_t *` | Bandera de parada y hallazgo del proceso |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `uint64_t` | Llaves que probó este hilo |
| `*shared` | `thread_shared_t` | Llave e iteraciones si este hilo la halló; `stop` levantada |

**Descripción.** Cuerpo de la región paralela. El hilo t del proceso r es el trabajador global
g = r·T + t y prueba g, g + pT, g + 2pT, … con su propio `workspace`. Cada `POLL_INTERVAL` llaves
revisa la bandera atómica `stop`; el hilo 0 además llama a `found_listener_poll` (`MPI_Test`) y
levanta la bandera si llegó un aviso de otro proceso. El hilo que halla la llave la guarda en una
sección `critical` y levanta la bandera.

### `main` (hibrido)

| Entradas | Tipo | Uso |
|---|---|---|
| `argc`, `argv` | `int`, `char **` | `-f`, `-k`, `-p`, `-b`, `-t` |
| `OMP_NUM_THREADS` | variable de entorno | Hilos por proceso (la lee el proceso 0) |

| Salidas | Tipo | Uso |
|---|---|---|
| retorno | `int` | `EXIT_SUCCESS`, o `EXIT_FAILURE` con parámetros o hilos inválidos |
| (stdout) | texto | Reporte y CSV del proceso 0, con `hilos = T` |

**Descripción.** Inicia MPI con hilos, prepara la búsqueda con `mpi_search_setup`, acuerda T y
pone la antena. Abre la región paralela (`#pragma omp parallel` con `reduction(+)` de las llaves
probadas) y, ya fuera de ella, el hilo principal hace el `found_listener_announce` si un hilo
halló la llave (FUNNELED). Cierra con `found_listener_finish` y `mpi_search_report`.

## `scripts/analizar.py` · Análisis de la campaña

Funciones de Python que convierten `resultados/campana_<host>_*.csv` en
`resultados/analisis_<host>.md` y las gráficas de `docs/img/`.

| Función | Entradas | Salidas | Descripción |
|---|---|---|---|
| `Group` (clase) | caso, programa, procesos, hilos, bits, llave real | objeto con listas de tiempos, iteraciones y llaves probadas | Repeticiones de un mismo caso; `time`, `finder_iterations` y `keys_tried` dan la **mediana** y `workers` = p·h |
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
