# Análisis de speedup y del comportamiento según la llave

Los números de este capítulo salen de la campaña preliminar en el i9-13980HX, con 28 bits, 5
repeticiones por caso y la mediana de cada caso. El detalle está en el Anexo 2 y en
`resultados/analisis_escu.md`.

Las fórmulas son las del curso (Barlas, 2015; Pacheco, 2011). El speedup es S = T_s / T_p, la
eficiencia E = S / w, el costo C = w · T_p y la efectividad F = S / C, con w = p · h
trabajadores. T_s es siempre el programa `secuencial` con la misma llave y los mismos bits.

## Lo teórico frente a lo medido

La tabla pone, para cada llave, el speedup por iteraciones que predice la teoría junto al
speedup medido con `-np 4`.

| Llave (28 bits) | | Naive | Intercalado | Bloque-cíclico | Maestro-trabajador |
|---|---|---|---|---|---|
| fácil · k = 134 217 729 | S_iter (teoría) | 6.7·10^7 | 4 | 4 | 3 |
| | S medido | 11 087 (aparente) | 3.19 | 3.21 | 2.30 |
| media · k = 167 772 160 | S_iter (teoría) | 5 | 4 | 4 | 3 |
| | S medido | 3.95 | 3.25 | 3.21 | 2.32 |
| difícil · k = 58 996 804 | S_iter (teoría) | 1 | 4 | 4 | 3 |
| | S medido | 0.79 | 3.28 | 3.27 | 2.35 |

S_iter, que solo cuenta iteraciones, coincide exactamente con la teoría del capítulo de enfoques
en todas las corridas. El reparto prueba las llaves que debe, y quien halla la llave lo hace en
el número de intentos previsto. La diferencia entre S_iter y el S medido es, entonces, pura
pérdida de velocidad por llave. La figura 11 muestra los speedups medidos.

![Speedup por llave con -np 4](img/speedup_llaves_escu.png)

Figura 11. Speedup de cada programa por llave con 4 procesos. La barra del naive con la llave
fácil está recortada.

## Por qué el naive es inconsistente

El tiempo del naive no depende de cuántas llaves hay, sino de dónde cae la llave dentro del
bloque del proceso que la tiene. Las tres llaves de la campaña lo muestran con los mismos 4
procesos.

La llave fácil, k = N/2 + 1, es la segunda del bloque del proceso 2. La halla en 2 intentos y
4 ms, que son casi todo arranque y la primera ronda de sondeo. El secuencial tarda 44.8 s, así
que el speedup es cercano a 11 000. Con 56 bits sería del orden de 10^16.

La llave media, k = N/2 + N/8, cae a la mitad del bloque del proceso 2, que recorre N/8 llaves.
La teoría da S_iter = 5, pero se midió S = 3.95. La razón es que los 4 procesos activos van cada
uno al 79 % del ritmo del secuencial, y 5 × 0.79 ≈ 3.95.

La llave difícil vale cerca de 0.22·N y cae en el bloque del proceso 0. Ese proceso hace las
mismas k + 1 iteraciones que el secuencial, así que S_iter = 1 y se midió S = 0.79. Es el peor
caso: cuatro procesos tardan más que uno.

Entre llaves, el speedup del naive va de 0.79 a 11 000. Con las pruebas del enunciado pasa lo
mismo: b y c son vecinas, y una da S ≈ 1 mientras la otra da S ≈ 10^12.

## Por qué las alternativas son consistentes

El intercalado da speedups de 3.19 a 3.28 en las tres llaves, y el bloque-cíclico de 3.21 a
3.27. Es una variación menor al 3 %, frente a cuatro órdenes de magnitud en el naive.

El maestro-trabajador da de 2.30 a 2.35, también parejo, aunque por debajo de los otros dos
porque el maestro no busca. Con 3 trabajadores su techo es 3, y respecto a ese techo su
eficiencia es 2.32 / 3 = 0.77, igual a la de los otros.

En todos ellos, el que halla la llave hace ⌊k/p⌋ + 1 iteraciones o su equivalente por chunks.
Por eso el speedup ronda p sin importar dónde esté la llave.

## El superlineal es aparente

En esta campaña ningún programa alternativo superó S = w, y el análisis automático no levantó
ninguna alerta. El único S mayor que p es el del naive con la llave fácil, y es aparente por
tres razones.

La primera es que el naive no procesa llaves más rápido. El speedup de rendimiento compara
llaves por segundo: S_rend = (llaves probadas / T_p) / ((k + 1) / T_s). Su máximo en toda la
campaña es 3.37 con w = 4 y 10.05 con w = 16, así que S_rend nunca supera w. En el naive con la
llave media vale 3.16: el proceso 2 solo tuvo que recorrer 1/8 del espacio, no lo recorrió más
rápido.

La segunda es que se trata del efecto de búsqueda que se vio en el ejercicio 9 de la clase: la
llave queda al inicio de un bloque que no es el primero. Las otras dos causas de
superlinealidad no aplican. El cifrado mide 32 bytes y cabe en caché tanto en secuencial como en
paralelo, y no hay sobrecarga que se reduzca al partir el problema.

La tercera es que en promedio el efecto desaparece. Si la llave es uniforme entre 0 y N, el
proceso que la tiene recorre en promedio la mitad de N/p llaves. Entonces E[T_p] = N/(2p) contra
E[T_s] = N/2, y el speedup esperado del naive es p, igual que el del intercalado. Las llaves que
le dan S mucho mayor que p se compensan con las que le dan S = 1.

Si un enfoque alternativo diera S > p, habría que revisar varias cosas en orden. Primero, que
T_s sea de la misma llave y los mismos bits, y que las banderas de compilación coincidan.
Después, que el secuencial no se haya medido con la máquina ocupada. Por último, que el reparto
no se salte llaves, lo que se ve si S_iter difiere de la teoría, y que T_p no se mida después del
aviso.

## Fugas de eficiencia

Con `-np 4`, el intercalado tiene E ≈ 0.81, y además S_rend ≈ S (3.26 frente a 3.25). Eso
significa que no se desperdician llaves: cada proceso prueba llaves más despacio que el
secuencial solo. La tabla ubica cada fuga en nuestro problema.

| Fuga | En nuestro problema |
|---|---|
| Parte secuencial (η_f) | Leer, cifrar y difundir el texto toma milisegundos. En el maestro-trabajador pesa el maestro entero, 1/p del cómputo |
| Comunicación (η_c) | Un `MPI_Test` cada 4096 llaves y un aviso al final: despreciable. En el maestro-trabajador, un pedido por chunk, oculto en parte por el pedido adelantado |
| Desbalance (η_b) | Nulo en los repartos estáticos, porque todos reciben N/p llaves. En el naive, respecto a la llave: p − 1 procesos trabajan donde no está |
| Redundancia (η_r) | Ninguna: cada llave se prueba una sola vez |
| Hardware | La que domina. Con un núcleo activo el i9 sube al turbo máximo; con 4 u 8 baja la frecuencia, y pasados 8 trabajadores se comparten núcleos por *hyperthreading* o entran los de eficiencia |

## Barrido de procesos, Amdahl y efectividad

La tabla resume el barrido con la llave media. La figura 12 lo compara con la recta ideal y con
la curva de Amdahl.

| Programa | p | T_p (s) | S | E | C (s) | F |
|---|---|---|---|---|---|---|
| secuencial | 1 | 56.57 | 1.00 | 1.00 | 56.57 | 0.018 |
| intercalado | 2 | 32.57 | 1.74 | 0.87 | 65.13 | 0.027 |
| intercalado | 4 | 17.41 | 3.25 | 0.81 | 69.65 | 0.047 |
| intercalado | 8 | 11.30 | 5.01 | 0.63 | 90.40 | 0.055 |
| maestro-trabajador | 2 | 67.13 | 0.84 | 0.42 | 134.27 | 0.006 |
| maestro-trabajador | 4 | 24.36 | 2.32 | 0.58 | 97.45 | 0.024 |
| maestro-trabajador | 8 | 13.48 | 4.20 | 0.52 | 107.82 | 0.039 |

![Barrido de procesos con la llave media](img/barrido_escu.png)

Figura 12. Speedup contra número de procesos con la llave media, con la recta ideal y la curva
de Amdahl de cada programa.

Para Amdahl se toma el intercalado con p = 4. La fracción paralelizable es
f_p = (1 − 1/3.25) / (1 − 1/4) = 0.923, así que f_s = 0.077 y el techo 1/f_s ronda 13. Con esa
fracción, Amdahl predice 5.20 para p = 8 y se midieron 5.01, un 3.7 % menos. Para p = 2 predice
1.86 y se midieron 1.74.

Esa f_s no es código secuencial. Es la pérdida de frecuencia al activar más núcleos, que crece
con p en vez de quedarse fija. Por eso Amdahl, calibrado en p = 4, sobrestima los extremos.

El maestro-trabajador no sigue a Amdahl, porque su pérdida es estructural: con p procesos tiene
p − 1 trabajadores. Con p = 2 hay uno solo y es más lento que el secuencial (S = 0.84). Con p = 8
el maestro pesa 1/8 y el speedup sube a 4.20, muy por encima de los 2.98 que predice Amdahl con
la f_s de p = 4. Su ideal es S = p − 1, y su eficiencia se acerca a 1 cuando p crece.

La ley de Gustafson describe mejor la fuerza bruta, porque el problema crece solo. Con S ≈ 3.25
se cubren 3.25 veces más llaves en el mismo tiempo, es decir, log2(3.25) ≈ 1.7 bits más. Con el
híbrido de 16 trabajadores (S ≈ 10) son unos 3.3 bits más. Como f_s es casi 0, el speedup
escalado de Gustafson tiende a p.

La efectividad sigue creciendo hasta el p más grande que se midió: 0.055 con 8 procesos en el
intercalado y 0.105 con el híbrido de 16 trabajadores. Todavía conviene agregar trabajadores,
aunque la eficiencia baje de 0.87 a 0.63. Si se exige una eficiencia de al menos 0.8, el punto
adecuado es p = 4.

## El híbrido frente a la versión sin OpenMP

La tabla da el speedup y la eficiencia del híbrido con 4 procesos y 1, 2 y 4 hilos por proceso.
La figura 13 muestra el tiempo y el speedup contra los hilos.

| Llave | h = 1 (w = 4) | h = 2 (w = 8) | h = 4 (w = 16) |
|---|---|---|---|
| fácil | S = 3.32 · E = 0.83 | S = 5.69 · E = 0.71 | S = 11.08 · E = 0.69 |
| media | S = 3.37 · E = 0.84 | S = 5.75 · E = 0.72 | S = 9.75 · E = 0.61 |
| difícil | S = 3.35 · E = 0.84 | S = 5.91 · E = 0.74 | S = 9.92 · E = 0.62 |

![Tiempo y speedup del híbrido contra los hilos](img/hibrido_escu.png)

Figura 13. Tiempo y speedup del híbrido con 4 procesos según los hilos por proceso.

Con un hilo, el híbrido es el intercalado y tarda entre 0.96 y 0.98 veces lo mismo: 13.50 s
frente a 14.04 s con la llave fácil, 16.78 frente a 17.41 con la media y 6.01 frente a 6.14 con
la difícil.

Con más hilos, el tiempo baja de forma pareja en las tres llaves. El speedup ronda 5.7 a 5.9 con
2 hilos y 9.8 a 11.1 con 4. S_iter vale exactamente 8 y 16, y S_rend nunca supera w.

Frente a `intercalado -np 8`, que usa los mismos 8 trabajadores pero todos MPI, el híbrido de 4
procesos con 2 hilos tarda 9.84 s contra 11.30 s. Es un poco mejor porque los hilos comparten el
proceso y no hay ocho procesos sondeando MPI.

La eficiencia baja con 16 trabajadores porque el i9 tiene 8 núcleos de rendimiento. Pasados 8
trabajadores, los demás comparten núcleo por *hyperthreading* o caen en núcleos de eficiencia,
que son más lentos. Como el reparto es estático, todos esperan al más lento. Aun así, el speedup
crece de forma consistente con w.

## De 28 a 56 bits

Al ritmo medido del secuencial, 167 772 161 llaves en 56.57 s, se prueban unos 2.97·10^6
llaves por segundo. Recorrer las 2^56 llaves tomaría 2^56 / 2.97·10^6 ≈ 2.43·10^10 s, unos 770
años.

El intercalado con 4 procesos (S = 3.25) bajaría ese tiempo a unos 237 años, y el híbrido de 16
trabajadores (S ≈ 9.75) a unos 79 años. Para recorrer el espacio completo en un día harían falta
2.43·10^10 / 86 400 ≈ 281 000 núcleos con eficiencia 1.

Como los presets escalan con N, S_iter con 28 bits es el mismo que con 56. Las únicas
excepciones son las llaves c y fácil del naive, que valen cerca de N/4. Por eso estas mediciones
representan el problema completo.
