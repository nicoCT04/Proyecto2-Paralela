# Introducción

El *Data Encryption Standard* (DES) protege un texto con una llave de 56 bits. Hoy esa llave es
demasiado corta: probar las 2^56 ≈ 7.2·10^16 combinaciones está al alcance de una máquina
dedicada (EFF, 1998), y por eso el NIST retiró el estándar en 2005 (NIST, 2005). Ese mismo
ataque, la **fuerza bruta**, es un problema ideal para estudiar la programación paralela: cada
llave se prueba de forma independiente, así que el trabajo se reparte casi sin comunicación, y
sin embargo la forma de repartirlo decide si el speedup es excelente o nulo.

En este proyecto se cifra un texto con DES y una llave privada, y se recupera la llave probando
llaves hasta que el texto descifrado contiene una frase clave conocida de antemano
(`"es una prueba de"`). La búsqueda se implementa en C con OpenSSL para DES, Open MPI para el
paralelismo entre procesos (MPI Forum, 2023) y, como extra, OpenMP para el paralelismo entre
hilos (OpenMP ARB, 2021).

## Objetivos

- Implementar y modificar un programa paralelo con MPI en C.
- Descifrar un texto cifrado con DES por fuerza bruta: primero de forma secuencial y después con
  el reparto *naive* del programa base del curso.
- Proponer y comparar enfoques alternativos de reparto, y medir su speedup, eficiencia, costo y
  efectividad con las fórmulas del curso (Amdahl, 1967; Gustafson, 1988).
- Explicar por qué el naive tiene un comportamiento inconsistente según la llave, y demostrar que
  el proyecto **no es superlineal**: los S > p que aparecen son un efecto de búsqueda.

## Qué se hizo

| Programa | Qué hace | Capítulo |
|---|---|---|
| `cifrar` | Cifra y descifra un `.txt` con una llave (Parte B.1) | Parte A y pruebas B |
| `secuencial` | Fuerza bruta en un solo proceso (Parte A.3); da el T_s | Parte A |
| `naive` | Bloques contiguos, uno por proceso (Parte B.2) | Pruebas B y enfoques |
| `intercalado` | El proceso r prueba r, r + p, r + 2p, … | Enfoques |
| `ciclico` | Chunks de 2^c llaves en round-robin | Enfoques |
| `maestro_trabajador` | El proceso 0 reparte chunks bajo demanda | Enfoques |
| `hibrido` | Intercalado con p procesos MPI y T hilos de OpenMP (extra) | Enfoques |

Todos comparten el mismo arranque, el mismo aviso de llave y el mismo reporte (módulo
`search_mpi`), y escriben una línea CSV con el mismo formato. Los scripts `verificar.sh`,
`campana.sh` y `analizar.py` comprueban que todo halla la llave real, corren la campaña de
mediciones (5 repeticiones por caso, mediana) y generan las tablas y gráficas del análisis.

Los extras que se abordaron son **tres o más enfoques alternativos** (se hicieron tres) y
**OpenMP**. El clúster en red local se descartó: exige las tres máquinas a la vez, con la misma
versión de Open MPI y el mismo sistema operativo, y mezclar macOS con Linux lo volvía frágil para
el tiempo disponible.

## Estructura del informe

1. **Antecedentes (Parte A):** el algoritmo DES y su diagrama de flujo, las rutinas de la base
   (`encrypt_des`, `decrypt_des`, `try_key`, `memcpy`, `strstr`) y las primitivas MPI del aviso
   de llave.
2. **Desarrollo (Parte B):** las pruebas B.1 y B.2 del naive con las llaves del enunciado, los
   enfoques alternativos con su análisis teórico y el híbrido.
3. **Resultados:** la bitácora de pruebas (Anexo 2) y el análisis de speedup: comparación por
   llave, barrido de procesos, Amdahl, Gustafson y la discusión del superlineal.
4. **Conclusiones y recomendaciones**, referencias y el catálogo de funciones (Anexo 1).
