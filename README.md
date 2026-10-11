# Proyecto 2 · Fuerza bruta sobre DES

Computación Paralela y Distribuida · UVG · Ing. Juan Luis García
Esteban Cárcamo (23016) · Nicolás Concuá (23197) · Diego López (23747)

Se cifra un texto con DES y una llave privada, y se recupera la llave probando
todas las combinaciones posibles (2^56). Se sabe que una llave es la correcta
porque el texto descifrado contiene una frase clave conocida de antemano.

## Requisitos

| Paquete | Arch / CachyOS | Ubuntu / Debian | macOS (Homebrew) |
|---|---|---|---|
| Compilador C, make | `sudo pacman -S base-devel` | `sudo apt install build-essential` | `xcode-select --install` |
| OpenSSL (DES) | `sudo pacman -S openssl` | `sudo apt install libssl-dev` | `brew install openssl@3` |
| Open MPI | `sudo pacman -S openmpi` | `sudo apt install openmpi-bin libopenmpi-dev` | `brew install open-mpi` |

`rpc/des_crypt.h`, que usa el `bruteforce.c` del curso, ya no existe en Linux
moderno; se reemplazó por la API DES de OpenSSL (`<openssl/des.h>`, `-lcrypto`).
En macOS el `Makefile` toma OpenSSL de Homebrew (`brew --prefix openssl@3`).

## Compilar

```bash
make          # compila cifrar y secuencial (gcc) y los programas MPI (mpicc)
make clean    # borra lo compilado
```

## Programas

Todos aceptan los mismos parámetros:

| Opción | Significado | Por defecto |
|---|---|---|
| `-f <archivo.txt>` | texto plano a cifrar | obligatorio |
| `-k <llave>` | llave privada: número (`123456`, `123456L`, `0x1E240`) o preset | obligatorio |
| `-p "<frase>"` | frase clave que confirma el descifrado | `"es una prueba de"` |
| `-b <bits>` | espacio de búsqueda de 2^bits llaves (8..56) | `56` |
| `-t <segundos>` | (búsquedas) límite de tiempo; al vencer se reporta «no encontrada» | `0` (sin límite) |
| `-c <bits>` | (solo `ciclico` y `maestro_trabajador`) chunk de 2^bits llaves, 8..30 y menor que `-b` | `16` |
| `-o <archivo>` | (solo `cifrar`) guarda el texto cifrado en binario | — |

Presets de llave (con N = 2^bits, así se escalan si se reduce `-b`):

| Preset | Valor | Con 56 bits |
|---|---|---|
| `a` | 123456 | 123456 |
| `b` | N/4 − 1 | 18014398509481983 |
| `c` | N/4 | 18014398509481984 |
| `facil` | N/2 + 1 | 36028797018963969 |
| `media` | N/2 + N/8 | 45035996273704960 |
| `dificil` | ⌈N/7 + N/13⌉ | 15836833854489657 |

### `cifrar`: cifrar y descifrar desde un `.txt` (Parte B.1)

```bash
./bin/cifrar -f data/texto.txt -k 42 -o resultados/cifrado_42.bin
```

Muestra el texto, el cifrado en hexadecimal, el descifrado con la misma llave y
qué sale con una llave errónea.

### `secuencial`: fuerza bruta en un solo proceso (Parte A.3)

```bash
./bin/secuencial -f data/texto.txt -k 123456
./bin/secuencial -f data/texto.txt -k facil -b 24
```

Imprime el archivo, la frase clave, la llave posible, el texto descifrado y el
tiempo; al final deja una línea `CSV,...` para la bitácora:

```
CSV,programa,procesos,hilos,bits,llave_real,llave_hallada,proceso,iteraciones,llaves_totales,tiempo_s
```

Si se agota el límite `-t`, `llave_hallada` es `NA` y `proceso` es `-1`.

### `naive`: fuerza bruta con MPI en bloques contiguos (Parte B.2)

```bash
mpirun -np 4 ./bin/naive -f data/texto.txt -k 123456
mpirun -np 4 ./bin/naive -f data/texto.txt -k media -b 24
mpirun -np 4 ./bin/naive -f data/texto.txt -k b -t 120
```

Es la versión del `bruteforce.c` del curso: el espacio de llaves se parte en
bloques contiguos, uno por proceso. Solo el proceso 0 lee el archivo y cifra;
el resto recibe el problema con `MPI_Bcast`. Quien halla la llave avisa a todos
con `MPI_Send`, y cada proceso lo detecta con `MPI_Irecv` + `MPI_Test` (cada
4096 llaves) y lo cierra con `MPI_Wait`.

En Linux, si `mpirun` reclama falta de *slots*, se agrega `--oversubscribe`
(no para medir tiempos).

### Enfoques alternativos al naive

Los tres usan el mismo arranque, el mismo aviso de llave y el mismo reporte
que el naive (módulo `search_mpi`); solo cambia qué llaves prueba cada proceso.

| Programa | Idea | Ejemplo |
|---|---|---|
| `intercalado` | el proceso r prueba r, r+p, r+2p, …: cualquier llave cae en ≈ k/p intentos | `mpirun -np 4 ./bin/intercalado -f data/texto.txt -k media -b 24` |
| `ciclico` | chunks de 2^c llaves repartidos en round-robin; dentro de cada chunk, en orden | `mpirun -np 4 ./bin/ciclico -f data/texto.txt -k media -b 24 -c 16` |
| `maestro_trabajador` | el proceso 0 reparte chunks bajo demanda y los procesos 1..p−1 buscan (mínimo 2 procesos) | `mpirun -np 4 ./bin/maestro_trabajador -f data/texto.txt -k media -b 24 -c 16` |

El maestro-trabajador pide el siguiente chunk por adelantado para ocultar la
ida y vuelta con el maestro, y respeta `-t` con resolución de un chunk.

## Mediciones

```bash
scripts/medir_longitudes.sh         # A.3: secuencial con llaves de 16..26 bits
scripts/pruebas_b2.sh               # B.2: llaves a, b y c, secuencial vs naive -np 4
NP=4 LIMIT=120 scripts/pruebas_b2.sh  # mismas pruebas con procesos y límite explícitos
scripts/verificar.sh                # todos los programas hallan la llave real (-b 20, -np 2 3 4)
scripts/campana.sh                  # campaña completa (BITS=28, 5 repeticiones)
BITS=24 REPS=1 scripts/campana.sh   # campaña corta para probar
```

Los resultados quedan en `resultados/secuencial_longitudes.csv` y
`resultados/pruebas_b2.csv`. Con 56 bits las llaves b y c no terminan en el
secuencial, así que el script usa el límite `-t` y las repite con `-b 28`.

`verificar.sh` imprime `OK` o `FALLA` por caso y sale con código distinto de 0
si algo falla. `campana.sh` se configura por variables de entorno (`BITS`,
`REPS`, `NP`, `NP_LIST`, `KEYS`, `HYBRID_THREADS`, `MPIRUN`), imprime una
estimación de cuánto va a tardar y deja `resultados/campana_<host>_<fecha>.csv`
(caso, repetición y las columnas del CSV) y `resultados/maquina_<host>.txt`.

## Estructura

```
include/  des_utils.h   DES con OpenSSL: encrypt_des, decrypt_des, try_key
          options.h     parámetros y presets de llave
          problem.h     carga del .txt, cifrado inicial y reporte
          search_mpi.h  arranque MPI, aviso de llave y reporte comunes
src/      cifrar.c  secuencial.c  naive.c  intercalado.c  ciclico.c
          maestro_trabajador.c  (+ implementación de los .h)
scripts/  medir_longitudes.sh  pruebas_b2.sh  verificar.sh  campana.sh
data/     texto.txt     "Esta es una prueba de proyecto 2"
resultados/             CSV de las corridas
```
