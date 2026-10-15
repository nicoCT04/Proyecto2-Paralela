# Conclusiones y recomendaciones

## Conclusiones

El reparto decide el speedup, no el número de procesos. Con el mismo código de búsqueda y los
mismos 4 procesos, el naive va de S = 0.79 con la llave difícil a S = 11 000 aparente con la
fácil, mientras el intercalado y el bloque-cíclico dan de 3.19 a 3.28 con cualquier llave. El
tiempo del naive depende de dónde cae la llave dentro del bloque de quien la tiene; el de los
repartos alternativos solo depende de k/p.

Una sola llave no basta para comparar algoritmos. Con k = 60 000 000 y 26 bits, el naive le ganó
al intercalado (3.71 s frente a 5.87 s) porque esa llave cae en el último cuarto del espacio,
donde el naive gana siempre; con una llave del primer cuarto pasa lo contrario. Sobre todas las
llaves, cada uno gana en la mitad y en promedio empatan, y con 16 llaves al azar el speedup del
naive fue de 0.83 a 7.83 contra 3.00 a 3.53 del intercalado. Lo que gana un enfoque alternativo
es consistencia, no un mejor promedio.

El proyecto no es superlineal. Los S mayores que p del naive son un efecto de búsqueda: el proceso
que tiene la llave empieza cerca de ella, pero no prueba llaves más rápido, y el speedup de
rendimiento nunca pasó de p en ningún programa. El maestro-trabajador queda en S ≈ 2.3 con 4
procesos, 0.77 de su techo de p − 1 = 3 porque el maestro no busca, y a cambio balancea solo la
carga cuando los núcleos son desiguales.

La parte secuencial de Amdahl no es código secuencial, sino pérdida efectiva: el turbo que baja
con más núcleos activos, los núcleos de eficiencia, el sondeo de MPI y, en el maestro-trabajador,
el propio maestro. Por eso Amdahl, calibrado con un valor de p, no predice bien los demás. La
fuerza bruta escala al estilo de Gustafson: con w trabajadores se cubren log2(w) bits más en el
mismo tiempo, aunque las 2^56 llaves seguirían tomando unos 770 años en un núcleo.

El híbrido MPI + OpenMP es consistente con la versión sin OpenMP: con un hilo tarda de 0.96 a 0.98
del intercalado, y con 2 y 4 hilos el speedup sube a cerca de 5.8 y de 10, parejo en las tres
llaves. Con 8 trabajadores queda algo mejor que el intercalado puro, siempre que los procesos no
queden atados a un núcleo.

## Retos y cómo se resolvieron

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
| Una comparación con una sola llave favorecía al naive | Fórmulas de intentos por programa, prueba con 16 llaves al azar y un visualizador que muestra qué llaves prueba cada proceso |

## Aprendizajes

Un speedup aislado no dice nada sin el caso que se midió. Hay que reportar la llave, los bits, el
T_s del secuencial real y la mediana de varias corridas, y comparar con varias llaves. El speedup
por iteraciones y el de rendimiento separan lo que aporta el reparto de lo que aporta el hardware
y la suerte de dónde cae la llave.

Cuando la comunicación es mínima, los detalles del lanzador y del hardware pesan tanto como el
algoritmo: el *binding* de `mpirun`, el turbo y la mezcla de núcleos de rendimiento y de
eficiencia cambiaron los resultados más que el propio reparto. Y un protocolo de mensajes
necesita invariantes explícitas, porque los casos borde, como tener pocos chunks o hallar la llave
al final, son justo los que no aparecen con los parámetros por defecto.

## Recomendaciones

Para fuerza bruta con llaves de distribución desconocida conviene un reparto intercalado o
bloque-cíclico, que dan el speedup esperado para cualquier llave sin comunicación extra. Si los
núcleos son desiguales o la máquina es compartida, conviene el maestro-trabajador, siempre que
haya suficientes procesos para que el maestro pese poco.

Al medir, conviene usar el secuencial real con las mismas banderas de compilación, la máquina sin
carga, al menos 5 repeticiones y varias llaves repartidas por todo el espacio. Cualquier S > p
debe tratarse como un error hasta explicarlo: revisar que T_s sea de la misma llave y bits, que el
reparto no se salte llaves (S_iter igual a la teoría) y que T_p no se mida después del aviso. En
los programas híbridos hay que revisar el *binding* con `mpirun --report-bindings` y no pedir más
hilos que núcleos de rendimiento.

Para atacar los 56 bits reales, el paso siguiente es un clúster o el uso de aceleradores como las
GPU: el problema es vergonzosamente paralelo y escala con el número de núcleos.
