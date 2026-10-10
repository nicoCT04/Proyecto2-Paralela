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
make          # compila bin/cifrar y bin/secuencial
make clean    # borra lo compilado
```

## Programas

Ambos aceptan los mismos parámetros:

| Opción | Significado | Por defecto |
|---|---|---|
| `-f <archivo.txt>` | texto plano a cifrar | obligatorio |
| `-k <llave>` | llave privada: número (`123456`, `123456L`, `0x1E240`) o preset | obligatorio |
| `-p "<frase>"` | frase clave que confirma el descifrado | `"es una prueba de"` |
| `-b <bits>` | espacio de búsqueda de 2^bits llaves (8..56) | `56` |
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
tiempo; al final deja una línea `CSV,...` para la bitácora.

## Mediciones

```bash
scripts/medir_longitudes.sh         # A.3: secuencial con llaves de 16..26 bits
```

Los resultados quedan en `resultados/secuencial_longitudes.csv`.

## Estructura

```
include/  des_utils.h   DES con OpenSSL: encrypt_des, decrypt_des, try_key
          options.h     parámetros y presets de llave
          problem.h     carga del .txt, cifrado inicial y reporte
src/      cifrar.c  secuencial.c  (+ implementación de los .h)
scripts/  medir_longitudes.sh
data/     texto.txt     "Esta es una prueba de proyecto 2"
resultados/             CSV de las corridas
```
