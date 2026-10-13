# Conclusiones y recomendaciones

## Conclusiones

1. **El reparto decide el speedup, no el número de procesos.** Con el mismo código de búsqueda y
   los mismos 4 procesos, el naive va de S ≈ 1 (llaves a, b y difícil) a S aparentes de 10^7
   (llaves c y fácil con 28 bits; ≈ 10^16 con 56), mientras que el intercalado y el bloque-cíclico
   dan S ≈ p con **cualquier** llave. El tiempo del naive depende de dónde cae la llave dentro del
   bloque del proceso que la tiene; el de los repartos alternativos, solo de k/p.
2. **El proyecto no es superlineal.** Los S > p del naive son un **efecto de búsqueda**: el
   proceso que tiene la llave empieza cerca de ella, pero no prueba llaves más rápido. El speedup
   de rendimiento (llaves por segundo) nunca pasa de p en ningún programa, y si se promedia sobre
   todas las llaves el speedup esperado del naive es p, igual que el del intercalado. El análisis
   automático lo comprueba en cada campaña: ningún enfoque alternativo da S > w.
3. **Lo que gana un enfoque alternativo es consistencia.** El intercalado y el bloque-cíclico
   sostienen una eficiencia alta y estable en las tres llaves; el maestro-trabajador queda cerca
   de p − 1 porque el maestro no busca, a cambio de balancear solo la carga cuando los núcleos son
   desiguales.
4. **La «parte secuencial» de Amdahl no es código secuencial.** Leer y cifrar el texto tarda
   milisegundos; la f_s que se despeja del speedup medido es pérdida efectiva: el turbo que baja
   con más núcleos activos, los núcleos de eficiencia, el sondeo de MPI y, en el
   maestro-trabajador, el propio maestro. Por eso Amdahl, calibrado con un p, no predice bien los
   demás.
5. **La fuerza bruta escala al estilo de Gustafson.** El trabajo crece solo (cada bit duplica el
   espacio), y con w trabajadores en el mismo tiempo se cubren log2(w) bits más. Recorrer las
   2^56 llaves en un día exigiría unos 270 000 núcleos al ritmo medido de un solo núcleo.
6. **El híbrido MPI + OpenMP da un speedup consistente con la versión sin OpenMP.** Con un hilo
   es el intercalado; con más hilos el tiempo baja de forma pareja en las tres llaves y queda
   cerca del intercalado puro con los mismos trabajadores, siempre que los procesos no queden
   atados a un núcleo.

## Retos y cómo se resolvieron con Open MPI

| Reto | Solución |
|---|---|
| `rpc/des_crypt.h` ya no existe en glibc moderna | DES de OpenSSL (`DES_ecb_encrypt`), verificado contra `openssl enc -des-ecb` |
| Errores impresos por todos los procesos y procesos colgados | Solo el proceso 0 lee y valida; difunde un encabezado de estado con `MPI_Bcast` y todos salen limpios |
| El aviso de llave no llegaba mientras se calculaba | `MPI_Irecv` antes de buscar y `MPI_Test` cada 4096 llaves, que hace avanzar la recepción |
| La prueba b con 56 bits no termina (≈ 150 años) | Límite `-t` con un centinela que el proceso 0 difunde al vencer, y la misma prueba con 28 bits |
| Medir con 56 bits es imposible | Presets que escalan con N = 2^bits: la llave conserva su posición relativa y S_iter |
| El maestro-trabajador se colgaba si los chunks se acababan antes de hallar la llave | Protocolo con dos invariantes: una respuesta por pedido y un único mensaje final por trabajador |
| El híbrido no escalaba con más hilos | Open MPI 5 ata cada proceso a un núcleo: se lanza con `--bind-to none` |
| Varios hilos y un MPI sin soporte multihilo completo | `MPI_THREAD_FUNNELED`: solo el hilo principal llama a MPI |

## Aprendizajes

- Un speedup aislado no dice nada sin el caso que se midió: hay que reportar la llave, los bits,
  el T_s del secuencial de verdad y la mediana de varias corridas.
- El speedup de rendimiento y el speedup por iteraciones separan lo que aporta el paralelismo de
  lo que aporta la suerte de dónde cae la llave.
- Los detalles del lanzador (`mpirun`) y del hardware (núcleos de rendimiento y de eficiencia,
  turbo) pesan tanto como el algoritmo cuando la comunicación es mínima.
- Un protocolo de mensajes necesita invariantes explícitas; los casos borde (pocos chunks,
  hallazgo al final) son justamente los que no aparecen con los parámetros por defecto.

## Recomendaciones

- Para fuerza bruta con llaves de distribución desconocida, usar un reparto **intercalado** o
  **bloque-cíclico**: dan el speedup esperado para cualquier llave sin comunicación extra.
- Si los núcleos son desiguales o la máquina es compartida, usar **maestro-trabajador** con
  suficientes procesos para que el maestro pese poco ((p − 1)/p → 1).
- Medir siempre con el secuencial real, las mismas banderas de compilación, la máquina sin carga
  y al menos 5 repeticiones; tratar cualquier S > p como un error hasta explicarlo.
- En híbridos, revisar el *binding* (`mpirun --report-bindings`) y no pedir más hilos que núcleos
  de rendimiento.
- Para 56 bits reales, el paso siguiente es un clúster o aceleradores (GPU): el problema es
  vergonzosamente paralelo y escala con el número de núcleos.
