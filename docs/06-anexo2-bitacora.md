# Anexo 2 · Bitácora de pruebas y speedups

## Máquina y software

Mediciones **preliminares** en la laptop de Esteban (la campaña final se repite en la MacBook Pro
de Nico con el mismo procedimiento):

| | |
|---|---|
| CPU | Intel Core i9-13980HX: 24 núcleos (8 de rendimiento con *hyperthreading* + 16 de eficiencia), 32 hilos |
| Memoria | 16 GB |
| Sistema | CachyOS (Linux 7.2.9) |
| Compilador | GCC 16.2.1 con `-O2 -march=native -std=c11` (los mismos en todos los programas) |
| MPI y librerías | Open MPI 5.0.11 · OpenSSL 3.6.5 · OpenMP de GCC |

La ficha completa (`lscpu` y versión de `mpirun`) queda en `resultados/maquina_escu.txt`.

## Procedimiento

```bash
make clean && make
scripts/verificar.sh                                  # 126 casos, 0 fallas
BITS=28 REPS=5 NP=4 NP_LIST="2 4 8" HYBRID_THREADS="1 2 4" scripts/campana.sh
python3 scripts/analizar.py resultados/campana_escu_<fecha>.csv
python3 scripts/capturas.py                           # capturas de este anexo
```

- **Bits:** 28 (N = 2^28 = 268 435 456). Los presets escalan con N, así que cada llave conserva
  la posición relativa que tiene con 56 bits.
- **Llaves:** fácil (N/2 + 1 = 134 217 729), media (N/2 + N/8 = 167 772 160) y difícil
  (⌈N/7 + N/13⌉ = 58 996 804).
- **Repeticiones:** 5 por caso; se reporta la **mediana** (y el mínimo y el máximo).
- **Tiempo:** de pared. En los programas MPI, `MPI_Wtime` desde la barrera hasta que llega el
  aviso; en el secuencial, `clock_gettime(CLOCK_MONOTONIC)` alrededor de la búsqueda.
- **T_s:** la mediana del programa `secuencial` con la misma llave y los mismos bits, no el
  paralelo con un proceso.
- **Híbrido:** `mpirun --bind-to none -x OMP_NUM_THREADS -np 4` con 1, 2 y 4 hilos.

## Verificación

`scripts/verificar.sh` con `-b 20` corre cada programa con las 6 llaves (a, b, c, fácil, media y
difícil) y `-np` 2, 3 y 4; el híbrido además con 1 y 2 hilos, y el maestro-trabajador con pocos
chunks (`-b 18`, que agota los chunks antes de hallar la llave) y con `-np 1` (debe dar error
sin colgarse). Resultado: **126 casos, 0 fallas**.

![Verificación](capturas/anexo2-verificar.png)

*Final de `scripts/verificar.sh`: los últimos casos y el resumen.*

## Pruebas B.1 y B.2 (Nico)

Las corridas del cifrado (llave 42) y de las pruebas a, b y c del naive con `-np 4` están en el
capítulo de pruebas B, con sus capturas y `resultados/pruebas_b2.csv`. En resumen: a da S ≈ 0.9
(mediana de 5), b no termina con 56 bits (≈ 149 años en secuencial; con 28 bits, S = 0.96) y c la
halla el proceso 1 al primer intento (S aparente ≈ 3.9·10^12 con 56 bits).

## Campaña preliminar: tiempos (mediana de 5, en segundos)

140 corridas; las 140 hallaron la llave real. Mínimo y máximo de cada caso en
`resultados/analisis_escu.md`.

| Programa | p | h | fácil | media | difícil |
|---|---|---|---|---|---|
| secuencial (T_s) | 1 | 1 | 44.792 | 56.569 | 20.127 |
| naive | 4 | 1 | 0.004 | 14.326 | 25.321 |
| intercalado | 4 | 1 | 14.040 | 17.414 | 6.136 |
| ciclico (C = 2^16) | 4 | 1 | 13.942 | 17.645 | 6.152 |
| maestro_trabajador (C = 2^16) | 4 | 1 | 19.472 | 24.362 | 8.563 |
| hibrido | 4 | 1 | 13.499 | 16.783 | 6.009 |
| hibrido | 4 | 2 | 7.868 | 9.842 | 3.404 |
| hibrido | 4 | 4 | 4.044 | 5.801 | 2.029 |

## Speedups (S = T_s / T_p)

| Programa | w | fácil | media | difícil | E (media) |
|---|---|---|---|---|---|
| naive | 4 | 11 087 (aparente) | 3.95 | 0.79 | 0.99 |
| intercalado | 4 | 3.19 | 3.25 | 3.28 | 0.81 |
| ciclico | 4 | 3.21 | 3.21 | 3.27 | 0.80 |
| maestro_trabajador | 4 | 2.30 | 2.32 | 2.35 | 0.58 |
| hibrido, 1 hilo | 4 | 3.32 | 3.37 | 3.35 | 0.84 |
| hibrido, 2 hilos | 8 | 5.69 | 5.75 | 5.91 | 0.72 |
| hibrido, 4 hilos | 16 | 11.08 | 9.75 | 9.92 | 0.61 |

**Alertas del análisis: ninguna.** Ningún enfoque alternativo da S > w y ningún programa da
S_rend > w.

## Barrido de procesos (llave media)

| Programa | p = 2 | p = 4 | p = 8 |
|---|---|---|---|
| intercalado: T_p (s) / S | 32.565 / 1.74 | 17.414 / 3.25 | 11.301 / 5.01 |
| maestro_trabajador: T_p (s) / S | 67.133 / 0.84 | 24.362 / 2.32 | 13.477 / 4.20 |

## Capturas

Cada captura es la salida real de los comandos, tal como la imprimen los programas, tomada con
`scripts/capturas.py` en la misma máquina después de la campaña (los tiempos de una corrida
suelta varían unas décimas respecto a la mediana).

**Secuencial (T_s)**

![Secuencial](capturas/anexo2-secuencial.png)

**Naive**

![Naive](capturas/anexo2-naive.png)

*Fácil: el proceso 2 la halla al segundo intento. Media: el proceso 2 a la mitad de su bloque.
Difícil: el proceso 0, con las mismas iteraciones que el secuencial.*

**Intercalado**

![Intercalado](capturas/anexo2-intercalado.png)

**Bloque-cíclico**

![Bloque-cíclico](capturas/anexo2-ciclico.png)

**Maestro-trabajador**

![Maestro-trabajador](capturas/anexo2-maestro_trabajador.png)

**Híbrido (4 procesos × 4 hilos)**

![Híbrido](capturas/anexo2-hibrido.png)

**Barrido de procesos**

![Barrido](capturas/anexo2-barrido.png)

*Intercalado y maestro-trabajador con 2 y 8 procesos (se muestran solo las líneas clave de cada
reporte).*
