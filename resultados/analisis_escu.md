# Análisis de la campaña · escu

- Archivos: campana_escu_20261012-2158.csv
- Corridas: 140 · bits: 28 · repeticiones por caso: hasta 5 (se usa la **mediana**)
- w = procesos · hilos; T_s = mediana del `secuencial` con la misma llave y bits.

**Alertas: ninguna.** Ningún enfoque alternativo pasa de S = w y ningún programa prueba llaves más rápido que w veces el secuencial.

## Verificación de llaves

Las 140 corridas hallaron la llave real (`llave_hallada == llave_real`).

## Comparación principal (-np 4)

### Llave fácil · k = 134217729 · 28 bits · T_s = 44.792 s

| Programa | p | h | w | T_p mediana (mín–máx) s | S | E | C (s) | F | S_iter | S_rend | Nota |
|---|---|---|---|---|---|---|---|---|---|---|---|
| secuencial | 1 | 1 | 1 | 44.792 (43.573–45.876) | 1.00 | 1.00 | 44.79 | 0.022 | 1.00 | 1.00 |  |
| naive | 4 | 1 | 4 | 0.004 (0.004–0.004) | 11087.11 | 2771.78 | 0.02 | 6.86·10^5 | 6.71·10^7 | 1.02 | aparente (búsqueda) |
| intercalado | 4 | 1 | 4 | 14.040 (13.536–14.710) | 3.19 | 0.80 | 56.16 | 0.057 | 4.00 | 3.19 |  |
| ciclico | 4 | 1 | 4 | 13.942 (13.709–14.393) | 3.21 | 0.80 | 55.77 | 0.058 | 4.00 | 3.22 |  |
| maestro_trabajador | 4 | 1 | 4 | 19.472 (19.327–19.628) | 2.30 | 0.58 | 77.89 | 0.030 | 3.00 | 2.30 |  |
| hibrido (1 hilos) | 4 | 1 | 4 | 13.499 (13.136–13.606) | 3.32 | 0.83 | 54.00 | 0.061 | 4.00 | 3.32 |  |
| hibrido (2 hilos) | 4 | 2 | 8 | 7.868 (7.704–7.907) | 5.69 | 0.71 | 62.94 | 0.090 | 8.00 | 5.69 |  |
| hibrido (4 hilos) | 4 | 4 | 16 | 4.044 (4.002–4.909) | 11.08 | 0.69 | 64.70 | 0.171 | 16.00 | 9.87 |  |

### Llave media · k = 167772160 · 28 bits · T_s = 56.569 s

| Programa | p | h | w | T_p mediana (mín–máx) s | S | E | C (s) | F | S_iter | S_rend | Nota |
|---|---|---|---|---|---|---|---|---|---|---|---|
| secuencial | 1 | 1 | 1 | 56.569 (55.410–56.901) | 1.00 | 1.00 | 56.57 | 0.018 | 1.00 | 1.00 |  |
| naive | 4 | 1 | 4 | 14.326 (14.196–14.714) | 3.95 | 0.99 | 57.31 | 0.069 | 5.00 | 3.16 |  |
| intercalado | 4 | 1 | 4 | 17.414 (17.315–17.469) | 3.25 | 0.81 | 69.65 | 0.047 | 4.00 | 3.26 |  |
| ciclico | 4 | 1 | 4 | 17.645 (17.469–18.046) | 3.21 | 0.80 | 70.58 | 0.045 | 4.00 | 3.21 |  |
| maestro_trabajador | 4 | 1 | 4 | 24.362 (24.282–24.836) | 2.32 | 0.58 | 97.45 | 0.024 | 3.00 | 2.32 |  |
| hibrido (1 hilos) | 4 | 1 | 4 | 16.783 (16.601–16.969) | 3.37 | 0.84 | 67.13 | 0.050 | 4.00 | 3.37 |  |
| hibrido (2 hilos) | 4 | 2 | 8 | 9.842 (9.679–9.957) | 5.75 | 0.72 | 78.74 | 0.073 | 8.00 | 5.73 |  |
| hibrido (4 hilos) | 4 | 4 | 16 | 5.801 (5.346–6.557) | 9.75 | 0.61 | 92.82 | 0.105 | 16.00 | 9.72 |  |

### Llave difícil · k = 58996804 · 28 bits · T_s = 20.127 s

| Programa | p | h | w | T_p mediana (mín–máx) s | S | E | C (s) | F | S_iter | S_rend | Nota |
|---|---|---|---|---|---|---|---|---|---|---|---|
| secuencial | 1 | 1 | 1 | 20.127 (19.657–20.448) | 1.00 | 1.00 | 20.13 | 0.050 | 1.00 | 1.00 |  |
| naive | 4 | 1 | 4 | 25.321 (25.166–25.460) | 0.79 | 0.20 | 101.28 | 0.008 | 1.00 | 3.19 |  |
| intercalado | 4 | 1 | 4 | 6.136 (6.124–6.151) | 3.28 | 0.82 | 24.54 | 0.134 | 4.00 | 3.29 |  |
| ciclico | 4 | 1 | 4 | 6.152 (6.123–6.422) | 3.27 | 0.82 | 24.61 | 0.133 | 4.00 | 3.28 |  |
| maestro_trabajador | 4 | 1 | 4 | 8.563 (8.529–8.606) | 2.35 | 0.59 | 34.25 | 0.069 | 3.00 | 2.36 |  |
| hibrido (1 hilos) | 4 | 1 | 4 | 6.009 (5.729–6.116) | 3.35 | 0.84 | 24.04 | 0.139 | 4.00 | 3.35 |  |
| hibrido (2 hilos) | 4 | 2 | 8 | 3.404 (3.368–3.440) | 5.91 | 0.74 | 27.23 | 0.217 | 8.00 | 5.91 |  |
| hibrido (4 hilos) | 4 | 4 | 16 | 2.029 (1.710–2.256) | 9.92 | 0.62 | 32.46 | 0.306 | 16.00 | 10.05 |  |

## Barrido de procesos (llave media)

### intercalado · T_s = 56.569 s

| Programa | p | h | w | T_p mediana (mín–máx) s | S | E | C (s) | F | S_iter | S_rend | Nota |
|---|---|---|---|---|---|---|---|---|---|---|---|
| intercalado | 2 | 1 | 2 | 32.565 (30.993–33.124) | 1.74 | 0.87 | 65.13 | 0.027 | 2.00 | 1.74 |  |
| intercalado | 4 | 1 | 4 | 17.414 (17.315–17.469) | 3.25 | 0.81 | 69.65 | 0.047 | 4.00 | 3.26 |  |
| intercalado | 8 | 1 | 8 | 11.301 (10.520–11.408) | 5.01 | 0.63 | 90.40 | 0.055 | 8.00 | 5.03 |  |

### maestro_trabajador · T_s = 56.569 s

| Programa | p | h | w | T_p mediana (mín–máx) s | S | E | C (s) | F | S_iter | S_rend | Nota |
|---|---|---|---|---|---|---|---|---|---|---|---|
| maestro_trabajador | 2 | 1 | 2 | 67.133 (63.794–71.017) | 0.84 | 0.42 | 134.27 | 0.006 | 1.00 | 0.84 |  |
| maestro_trabajador | 4 | 1 | 4 | 24.362 (24.282–24.836) | 2.32 | 0.58 | 97.45 | 0.024 | 3.00 | 2.32 |  |
| maestro_trabajador | 8 | 1 | 8 | 13.477 (13.134–14.366) | 4.20 | 0.52 | 107.82 | 0.039 | 7.01 | 4.21 |  |

## Amdahl y Gustafson

f_p = (1 − 1/S) / (1 − 1/p) con el S medido en -np 4; f_s = 1 − f_p; techo de Amdahl = 1/f_s. Esta «f_s» no es código secuencial (leer y cifrar tardan milisegundos): es la pérdida efectiva por comunicación, sondeo y hardware (turbo, núcleos de rendimiento y de eficiencia). En el maestro-trabajador incluye además al maestro, que no busca. Gustafson (tiempo fijo): en el mismo tiempo se prueban S veces más llaves, es decir, log2(S) bits más.

| Programa | Llave | p | S | f_p | f_s | Techo 1/f_s | S Gustafson | Bits extra |
|---|---|---|---|---|---|---|---|---|
| intercalado | facil | 4 | 3.19 | 0.915 | 0.085 | 11.8 | 3.75 | 1.67 |
| intercalado | media | 4 | 3.25 | 0.923 | 0.077 | 13.0 | 3.77 | 1.70 |
| intercalado | dificil | 4 | 3.28 | 0.927 | 0.073 | 13.7 | 3.78 | 1.71 |
| ciclico | facil | 4 | 3.21 | 0.918 | 0.082 | 12.2 | 3.75 | 1.68 |
| ciclico | media | 4 | 3.21 | 0.917 | 0.083 | 12.1 | 3.75 | 1.68 |
| ciclico | dificil | 4 | 3.27 | 0.926 | 0.074 | 13.5 | 3.78 | 1.71 |
| maestro_trabajador | facil | 4 | 2.30 | 0.754 | 0.246 | 4.1 | 3.26 | 1.20 |
| maestro_trabajador | media | 4 | 2.32 | 0.759 | 0.241 | 4.2 | 3.28 | 1.22 |
| maestro_trabajador | dificil | 4 | 2.35 | 0.766 | 0.234 | 4.3 | 3.30 | 1.23 |

Predicción de Amdahl para el barrido (llave media, f_p de -np 4):

| Programa | p | S medido | S Amdahl | Diferencia | Ideal del enfoque |
|---|---|---|---|---|---|
| intercalado | 2 | 1.74 | 1.86 | -6.4 % | 2 |
| intercalado | 4 | 3.25 | 3.25 | +0.0 % | 4 |
| intercalado | 8 | 5.01 | 5.20 | -3.7 % | 8 |
| maestro_trabajador | 2 | 0.84 | 1.61 | -47.7 % | 1 |
| maestro_trabajador | 4 | 2.32 | 2.32 | -0.0 % | 3 |
| maestro_trabajador | 8 | 4.20 | 2.98 | +40.9 % | 7 |

## Consistencia del híbrido

Con T = 1 el híbrido es el intercalado; con más hilos debe bajar de forma pareja en todas las llaves, y frente a `intercalado -np w` (los mismos trabajadores, todos MPI) debe quedar parecido.

| Llave | Hilos | w | T_p híbrido (s) | S híbrido | T_p intercalado -np w (s) | Híbrido / intercalado |
|---|---|---|---|---|---|---|
| facil | 1 | 4 | 13.499 | 3.32 | 14.040 | 0.96 |
| facil | 2 | 8 | 7.868 | 5.69 | — | — |
| facil | 4 | 16 | 4.044 | 11.08 | — | — |
| media | 1 | 4 | 16.783 | 3.37 | 17.414 | 0.96 |
| media | 2 | 8 | 9.842 | 5.75 | 11.301 | 0.87 |
| media | 4 | 16 | 5.801 | 9.75 | — | — |
| dificil | 1 | 4 | 6.009 | 3.35 | 6.136 | 0.98 |
| dificil | 2 | 8 | 3.404 | 5.91 | — | — |
| dificil | 4 | 16 | 2.029 | 9.92 | — | — |
