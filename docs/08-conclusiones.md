# Conclusiones y recomendaciones

## Conclusiones

El reparto decide el speedup, no el número de procesos. Con el mismo código de búsqueda y los
mismos 4 procesos, el naive va de S = 0.79 con la llave difícil a S = 11 000 aparente con la
fácil, que con 56 bits sería del orden de 10^16. En cambio, el intercalado y el bloque-cíclico dan
de 3.19 a 3.28 con cualquier llave. El tiempo del naive depende de dónde cae la llave dentro del
bloque de quien la tiene, y el de los repartos alternativos solo depende de k/p.

El proyecto no es superlineal. Los S mayores que p del naive son un efecto de búsqueda: el
proceso que tiene la llave empieza cerca de ella, pero no prueba llaves más rápido. El speedup
de rendimiento, en llaves por segundo, nunca pasó de p en ningún programa. Además, promediado
sobre todas las llaves, el speedup esperado del naive es p, igual que el del intercalado. El
análisis automático lo comprueba en cada campaña, y ningún enfoque alternativo dio S > w.

Lo que gana un enfoque alternativo es consistencia. El intercalado y el bloque-cíclico sostienen
una eficiencia de 0.80 a 0.82 en las tres llaves. El maestro-trabajador queda en S ≈ 2.3 con 4
procesos, que es 0.77 de su techo de p − 1 = 3, porque el maestro no busca. A cambio, balancea
solo la carga cuando los núcleos son desiguales.

La parte secuencial de Amdahl no es código secuencial. Leer y cifrar el texto toma milisegundos;
la f_s que se despeja del speedup medido es pérdida efectiva. Ahí entran el turbo que baja con
más núcleos activos, los núcleos de eficiencia, el sondeo de MPI y, en el maestro-trabajador, el
propio maestro. Por eso Amdahl, calibrado con un valor de p, no predice bien los demás.

La fuerza bruta escala al estilo de Gustafson. El trabajo crece solo, porque cada bit duplica el
espacio, y con w trabajadores se cubren log2(w) bits más en el mismo tiempo. Al ritmo medido de
un núcleo, 2.97·10^6 llaves por segundo, recorrer las 2^56 llaves tomaría unos 770 años, y
hacerlo en un día exigiría unos 281 000 núcleos.

El híbrido MPI + OpenMP da un speedup consistente con la versión sin OpenMP. Con un hilo es el
intercalado, con un tiempo de 0.96 a 0.98 del suyo. Con 2 y 4 hilos el speedup sube a cerca de
5.8 y de 10, de forma pareja en las tres llaves. Con 8 trabajadores queda incluso algo mejor que
el intercalado puro, siempre que los procesos no queden atados a un núcleo.

## Retos y cómo se resolvieron

La tabla resume los problemas que aparecieron durante el proyecto y cómo se resolvieron con Open
MPI y las herramientas del sistema.

| Reto | Solución |
|---|---|
| `rpc/des_crypt.h` ya no existe en glibc moderna | DES de OpenSSL (`DES_ecb_encrypt`), verificado contra `openssl enc -des-ecb` |
| Errores impresos por todos los procesos y procesos colgados | Solo el proceso 0 lee y valida; difunde un encabezado de estado con `MPI_Bcast` y todos salen limpios |
| El aviso de llave no llegaba mientras se calculaba | `MPI_Irecv` antes de buscar y `MPI_Test` cada 4096 llaves, que hace avanzar la recepción |
| La prueba b con 56 bits no termina (unos 150 años) | Límite `-t` con un centinela que el proceso 0 difunde al vencer, y la misma prueba con 28 bits |
| Medir con 56 bits es imposible | Presets que escalan con N = 2^bits: la llave conserva su posición relativa y S_iter |
| El maestro-trabajador se colgaba si los chunks se acababan antes de hallar la llave | Protocolo con dos invariantes: una respuesta por pedido y un único mensaje final por trabajador |
| El híbrido no escalaba con más hilos | Open MPI 5 ata cada proceso a un núcleo, así que se lanza con `--bind-to none` |
| Varios hilos con un MPI sin soporte multihilo completo | `MPI_THREAD_FUNNELED`: solo el hilo principal llama a MPI |

## Aprendizajes

Un speedup aislado no dice nada sin el caso que se midió. Hay que reportar la llave, los bits, el
T_s del secuencial real y la mediana de varias corridas. El speedup de rendimiento y el speedup
por iteraciones ayudan a separar lo que aporta el paralelismo de lo que aporta la suerte de dónde
cae la llave.

Cuando la comunicación es mínima, los detalles del lanzador y del hardware pesan tanto como el
algoritmo. El *binding* de `mpirun`, el turbo y la mezcla de núcleos de rendimiento y de
eficiencia cambiaron los resultados más que el propio reparto.

Un protocolo de mensajes necesita invariantes explícitas. Los casos borde, como tener pocos
chunks o hallar la llave al final, son justo los que no aparecen con los parámetros por defecto.

## Recomendaciones

Para fuerza bruta con llaves de distribución desconocida conviene un reparto intercalado o
bloque-cíclico. Ambos dan el speedup esperado para cualquier llave sin comunicación extra. Si los
núcleos son desiguales o la máquina es compartida, el maestro-trabajador es mejor opción, siempre
que haya suficientes procesos para que el maestro pese poco.

Al medir, conviene usar siempre el secuencial real con las mismas banderas de compilación, la
máquina sin carga y al menos 5 repeticiones. Cualquier S > p debe tratarse como un error hasta
explicarlo. En los programas híbridos hay que revisar el *binding* con `mpirun --report-bindings`
y no pedir más hilos que núcleos de rendimiento.

Para atacar los 56 bits reales, el paso siguiente es un clúster o el uso de aceleradores como las
GPU. El problema es vergonzosamente paralelo y escala con el número de núcleos.
