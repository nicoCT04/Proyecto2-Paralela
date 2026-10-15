# Introducción

El *Data Encryption Standard* (DES) protege un texto con una llave de 56 bits. Hoy esa llave es
demasiado corta: probar las 2^56 ≈ 7.2·10^16 combinaciones está al alcance de una máquina
dedicada (Electronic Frontier Foundation [EFF], 1998). Por eso el Instituto Nacional de Estándares
y Tecnología de Estados Unidos retiró el estándar en 2005 (National Institute of Standards and
Technology [NIST], 2005).

Ese ataque, la fuerza bruta, es un buen problema para estudiar la programación paralela. Cada
llave se prueba de forma independiente, así que el trabajo se reparte casi sin comunicación. Sin
embargo, la forma de repartirlo decide si el speedup es excelente o nulo.

En este proyecto se cifra un texto con DES y una llave privada. Después se recupera la llave
probando combinaciones hasta que el texto descifrado contiene una frase clave conocida, «es una
prueba de». La búsqueda está escrita en C, con OpenSSL para DES y Open MPI para repartir el
trabajo entre procesos (Message Passing Interface Forum [MPI Forum], 2023). Como extra, OpenMP reparte el trabajo de cada proceso
entre hilos (OpenMP Architecture Review Board [OpenMP ARB], 2021).

## Objetivos

El objetivo general es implementar y modificar un programa paralelo con MPI en C que descifre por
fuerza bruta un texto cifrado con DES. Primero se resuelve de forma secuencial y después con el
reparto naive del programa base del curso.

Sobre esa base se proponen enfoques alternativos de reparto. Cada uno se mide con las métricas
del curso: speedup, eficiencia, costo y efectividad, junto con las leyes de Amdahl (1967) y
Gustafson (1988).

El último objetivo es explicar por qué el naive se comporta de forma inconsistente según la
llave. Con esa explicación se demuestra que el proyecto no es superlineal: los speedups mayores
que p que aparecen son un efecto de búsqueda.

## Programas desarrollados

La tabla resume los siete programas del proyecto y el capítulo donde se discute cada uno.

| Programa | Qué hace | Capítulo |
|---|---|---|
| `cifrar` | Cifra y descifra un `.txt` con una llave (Parte B.1) | Parte A y pruebas B |
| `secuencial` | Fuerza bruta en un solo proceso (Parte A.3); da el T_s | Parte A |
| `naive` | Bloques contiguos, uno por proceso (Parte B.2) | Pruebas B y enfoques |
| `intercalado` | El proceso r prueba r, r + p, r + 2p, … | Enfoques |
| `ciclico` | Chunks de 2^c llaves en round-robin | Enfoques |
| `maestro_trabajador` | El proceso 0 reparte chunks bajo demanda | Enfoques |
| `hibrido` | Intercalado con p procesos MPI y T hilos de OpenMP (extra) | Enfoques |

Todos comparten el arranque, el aviso de llave y el reporte del módulo `search_mpi`, y escriben
una línea CSV con el mismo formato. Tres scripts completan el trabajo: `verificar.sh` comprueba
que cada programa halla la llave real, `campana.sh` corre las mediciones y `analizar.py` genera
las tablas y gráficas a partir de la mediana. Un visualizador web (`visualizer/`) corre los
programas reales y dibuja qué llaves prueba cada proceso.

De los extras opcionales se abordaron dos: tres enfoques alternativos y OpenMP. El clúster en
red local se descartó. Exigía las tres máquinas a la vez, con la misma versión de Open MPI y el
mismo sistema operativo, y mezclar macOS con Linux lo volvía frágil para el tiempo disponible.

## Estructura del informe

Los antecedentes (Parte A) describen DES, la búsqueda secuencial, las rutinas de la base y las
primitivas MPI del aviso de llave. El desarrollo (Parte B) presenta las pruebas del naive y los
enfoques alternativos con su análisis teórico. Los resultados comparan los programas por llave,
explican por qué una sola llave no basta para compararlos y aplican Amdahl y Gustafson. Cierran
las conclusiones, las referencias, el catálogo de funciones (Anexo 1) y la bitácora de pruebas
(Anexo 2).
