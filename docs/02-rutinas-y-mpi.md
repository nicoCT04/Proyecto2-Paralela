# Parte A.4 a A.6: Rutinas de la base y primitivas MPI

## A.4 Por qué no compila `rpc/des_crypt.h`

El `bruteforce.c` del curso incluye `<rpc/des_crypt.h>` y usa `ecb_crypt` y `des_setparity`.
Esas funciones eran parte de la implementación de Sun RPC que traía la biblioteca de C de GNU
(glibc). En glibc 2.32, publicada en 2020, se eliminó por completo Sun RPC junto con `rpcgen` y
sus cabeceras (Fedora Project, s. f.; Weimer, 2020). Desde entonces se recomienda la biblioteca
externa libtirpc, que no viene instalada por defecto.

El motivo fue que esa implementación no soportaba IPv6, y arreglarlo exigía romper la ABI
(Fedora Project, s. f.). Por eso, en una distribución moderna como Ubuntu 22.04, Arch o Fedora,
la línea `#include <rpc/des_crypt.h>` falla con «No such file or directory». En macOS tampoco
existe.

En lugar de depender de libtirpc, usamos la API DES de OpenSSL (`<openssl/des.h>`, enlazada con
`-lcrypto`). Está en cualquier sistema y permite verificar el resultado con
`openssl enc -des-ecb` (OpenSSL Project, s. f.). OpenSSL 3 marca esa API como obsoleta porque
DES ya no es seguro, pero la mantiene; el aviso se apaga con `-DOPENSSL_SUPPRESS_DEPRECATED`.
La tabla siguiente muestra qué rutina de nuestro módulo `des_utils` reemplaza a cada original.

| `bruteforce.c` (Sun RPC) | Nuestro proyecto (OpenSSL) |
|---|---|
| `encrypt(key, ciph, len)` | `encrypt_des(key, buffer, length)` |
| `decrypt(key, ciph, len)` | `decrypt_des(key, buffer, length)` |
| `tryKey(key, ciph, len)` | `try_key(key, cipher, length, phrase, workspace)` |
| `des_setparity(k)` | `DES_set_odd_parity(&k)` |
| `ecb_crypt(k, buf, len, DES_ENCRYPT)` | `DES_set_key_unchecked` + `DES_ecb_encrypt` por bloque |

## A.5 Las rutinas de la base

### `encrypt_des` y `decrypt_des`

Ambas llaman a la misma función interna, `run_des`, y solo cambian la dirección (`DES_ENCRYPT`
o `DES_DECRYPT`). Trabajan sobre el mismo buffer: el resultado sobrescribe la entrada.

Lo primero es armar la llave con `build_des_key`. La llave es un número de 56 bits, pero DES
pide 8 bytes en los que el bit menos significativo de cada uno es de paridad. Igual que el
`decrypt()` original, los 56 bits se reparten en grupos de 7: el byte i guarda los bits 7i a
7i + 6 en sus posiciones 1 a 7. Después, `DES_set_odd_parity` llena la posición 0 para que cada
byte tenga paridad impar.

Con la llave lista, `DES_set_key_unchecked` calcula las 16 subllaves de 48 bits con PC-1, las
rotaciones y PC-2. Usamos la versión sin revisión porque la normal rechaza las llaves débiles de
DES, y en fuerza bruta hay que probarlas todas. Por último, `DES_ecb_encrypt` procesa cada
bloque de 8 bytes con la misma agenda. La longitud siempre es múltiplo de 8 gracias al relleno
con ceros.

### `try_key`

Es el corazón de la búsqueda, porque prueba una llave candidata. Copia el cifrado al buffer
`workspace` con `memcpy` y le pone un `'\0'` al final para tratarlo como cadena de C. Luego lo
descifra con la llave candidata y busca la frase clave con `strstr`.

Si la frase aparece, la llave es la correcta. Con muy poca probabilidad podría ser un falso
positivo, y en ese caso el reporte lo marca. El `workspace` lo reserva una sola vez quien llama,
en lugar de pedir memoria en cada intento como hacía el original. Así se evitan millones de
llamadas a `malloc` y `free`.

### `memcpy`

`memcpy(destino, origen, n)` copia `n` bytes de un buffer a otro (ISO/IEC, 2011). Aquí es
indispensable porque `decrypt_des` descifra sobre el mismo buffer. Si se descifrara el cifrado
original, el primer intento fallido lo destruiría y no quedaría nada que probar con la llave
siguiente. Con la copia, el cifrado original queda intacto durante toda la búsqueda.

### `strstr`

`strstr(texto, frase)` devuelve un puntero a la primera aparición de `frase` dentro de `texto`,
o `NULL` si no aparece (ISO/IEC, 2011). Es la prueba de que la llave es correcta: con una llave
errónea, el descifrado es basura y la frase no aparece. Por eso el texto debe terminar en
`'\0'`, y por eso `problem_prepare` rechaza de entrada una frase que no esté en el texto
original. La figura 2 muestra la secuencia completa de un intento.

![Secuencia de un intento de llave](img/try-key-secuencia.png)

Figura 2. Lo que ocurre en cada llamada a `try_key`.

## A.6 Primitivas MPI del aviso de llave

El aviso de que alguien halló la llave usa cuatro primitivas de MPI (MPI Forum, 2023). La tabla
resume para qué sirve cada una.

| Primitiva | Tipo | Para qué la usamos |
|---|---|---|
| `MPI_Irecv` | No bloqueante | Dejar puesta una «antena» que reciba el aviso de llave encontrada |
| `MPI_Send` | Bloqueante | Que quien halla la llave avise a todos los procesos |
| `MPI_Wait` | Bloqueante | Cerrar la recepción pendiente antes de terminar |
| `MPI_Test` | No bloqueante | Revisar, sin detenerse, si ya llegó el aviso |

### `MPI_Irecv`

`MPI_Irecv(buf, count, datatype, source, tag, comm, &request)` inicia una recepción y regresa de
inmediato, sin esperar el mensaje. Devuelve un `MPI_Request` con el que después se consulta o se
espera. En el naive, cada proceso la publica antes de empezar a buscar:

```c
MPI_Irecv(found_msg, 3, MPI_UINT64_T, MPI_ANY_SOURCE, FOUND_TAG, MPI_COMM_WORLD, &request);
```

Con `MPI_ANY_SOURCE` el aviso se recibe venga de quien venga, y `found_msg` guarda la llave, el
proceso que la halló y sus iteraciones. Mientras tanto, el proceso sigue probando llaves; por
eso la llamamos la antena.

### `MPI_Send`

`MPI_Send(buf, count, datatype, dest, tag, comm)` envía un mensaje y no regresa hasta que el
buffer se puede reutilizar. El proceso que encuentra la llave envía el aviso a todos, incluido
él mismo, para que su propio `MPI_Irecv` también se complete y todos cierren igual. El mensaje
mide 24 bytes y cada destino ya tiene su recepción publicada, así que el envío termina casi de
inmediato.

### `MPI_Wait`

`MPI_Wait(&request, &status)` bloquea hasta que termina la operación no bloqueante asociada. Al
salir del bucle, todos los procesos hacen `MPI_Wait` sobre su antena. Así se aseguran de que
`found_msg` ya tiene el aviso antes de usarlo y de que no queda nada pendiente al llamar a
`MPI_Finalize`. Si un proceso agotó su bloque sin hallar nada, `MPI_Wait` lo deja esperando
hasta que otro avise.

### Por qué hace falta `MPI_Test`

El `bruteforce.c` original revisa una variable `found` en cada vuelta, pero esa variable solo
cambia si la biblioteca MPI hace avanzar la recepción. Open MPI no tiene, por defecto, un hilo
que procese mensajes en segundo plano: las comunicaciones solo avanzan dentro de llamadas a MPI.
Un proceso que solo ejecuta `try_key` nunca entra a MPI, así que el aviso no se entrega y el
proceso termina su bloque completo.

Por eso, cada `POLL_INTERVAL` = 4096 llaves, el naive llama a
`MPI_Test(&request, &done, MPI_STATUS_IGNORE)`. Esa llamada le da a MPI la oportunidad de
avanzar y, si el aviso ya llegó, `done` vale 1 y el proceso sale del bucle. Probar 4096 llaves
toma alrededor de 1 ms, así que el costo es despreciable y el retraso para enterarse es de ese
orden.

### Flujo de mensajes del naive con 4 procesos

En la figura 3, el proceso 1 encuentra la llave y envía el aviso a los 4 procesos. Cada uno lo
detecta en su siguiente `MPI_Test` y cierra con `MPI_Wait`. Al final, el proceso 0 reúne las
iteraciones con `MPI_Reduce` y reporta el resultado.

![Flujo de mensajes del naive](img/naive-mensajes.png)

Figura 3. Mensajes del naive con 4 procesos cuando el proceso 1 halla la llave.

Además del aviso, el naive usa `MPI_Bcast` para repartir el problema, porque solo el proceso 0
lee el archivo y cifra. Usa `MPI_Barrier` para que todos arranquen el reloj (`MPI_Wtime`) a la
vez y `MPI_Reduce` para sumar las llaves probadas. Cuando hay límite de tiempo (`-t`), el
proceso 0 envía el mismo aviso con la llave imposible `UINT64_MAX`, que el reporte traduce a «no
encontrada».
