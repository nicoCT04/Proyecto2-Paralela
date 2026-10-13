# Enfoques de paralelización

Los programas MPI resuelven el mismo problema con el mismo arranque, el mismo aviso de llave y el
mismo reporte, que comparten en el módulo `search_mpi`. Lo único que cambia entre ellos es qué
llaves prueba cada proceso y en qué orden. Este capítulo describe cada reparto y predice con una
fórmula quién halla la llave y en cuántas iteraciones. También discute qué gana y qué pierde
cada uno.

## Esqueleto común

Todos los repartos estáticos siguen el mismo esqueleto. Solo el proceso 0 lee y cifra el texto,
y luego lo difunde a los demás. Cada proceso deja puesta su antena de aviso antes de buscar y
arranca el reloj después de una barrera.

```text
proceso 0: lee parámetros y archivo, cifra el texto
MPI_Bcast(encabezado, cifrado, frase)            # errores: salen una vez y todos cierran
MPI_Irecv(aviso)                                 # la "antena", puesta antes de buscar
MPI_Barrier; inicio = MPI_Wtime()
para cada llave k del reparto de este proceso:
    cada POLL_INTERVAL = 4096 llaves: MPI_Test(antena); si llegó el aviso, salir
    si try_key(k): MPI_Send(aviso {k, rango, iteraciones}) a todos; salir
MPI_Wait(antena); T_p = MPI_Wtime() - inicio
MPI_Reduce(llaves probadas, SUMA) y el proceso 0 reporta
```

El sondeo cada 4096 llaves es necesario porque Open MPI no hace avanzar la recepción mientras el
proceso calcula. Su costo es despreciable, una llamada por cada 4096 descifrados. El aviso se
envía también a uno mismo, para que su propio `MPI_Irecv` se complete y el `MPI_Wait` final no
quede esperando.

En lo que sigue, N = 2^bits es el número de llaves, p el de procesos y k la llave buscada. Las
iteraciones son las llaves que probó el proceso que halló la llave. El secuencial prueba k + 1
llaves, así que cada reparto predice, sin necesidad de medir, un speedup por iteraciones
S_iter = (k + 1) / (iteraciones del que la halla).

La figura 8 compara qué llaves prueba cada proceso en los tres repartos estáticos, con un
espacio pequeño de 16 llaves.

![Qué llaves prueba cada proceso en los repartos estáticos](img/enfoques-reparto.png)

Figura 8. Llaves que prueba cada proceso en los repartos estáticos, con 16 llaves, p = 4 y
chunks de C = 2.

## Naive: bloques contiguos

El naive es el reparto del `bruteforce.c` del curso y sirve de referencia. El espacio se parte en
p bloques de N/p llaves, y el proceso r recorre el suyo de principio a fin.

```text
bloque = N / p
inicio = r · bloque;  fin = (r == p - 1) ? N : inicio + bloque
para k = inicio .. fin - 1: probar k
```

La llave la halla el proceso r = ⌊k / (N/p)⌋, tras k − r·N/p + 1 iteraciones. Por eso
S_iter = (k + 1) / (k − r·N/p + 1). Ese valor va de 1, cuando la llave cae en el bloque del
proceso 0, a cerca de N/p, cuando la llave es la primera de un bloque que no es el primero.

Su ventaja es la simpleza: recorre llaves contiguas y no necesita más comunicación que el aviso.
Su desventaja es que el speedup depende por completo de dónde cae la llave. Con llaves pequeñas,
que son las que más probablemente elige una persona, no gana nada.

La fuga de eficiencia que domina es el desbalance respecto a la llave. Los p − 1 procesos que no
la tienen trabajan en vano, y el que la tiene recorre, en promedio, la mitad de su bloque.

## Intercalado

En el intercalado, el proceso r prueba las llaves r, r + p, r + 2p, y así hasta el final. Todos
los procesos avanzan a la vez por las llaves bajas y por las altas.

```text
para k = r, r + p, r + 2p, ... < N: probar k
```

La llave la halla el proceso k mod p, tras ⌊k/p⌋ + 1 iteraciones. Entonces
S_iter = (k + 1) / (⌊k/p⌋ + 1), que vale casi exactamente p y nunca lo supera.

Su gran ventaja es que el speedup ronda p para cualquier llave, sin comunicación extra y
cambiando solo dos líneas respecto al naive. Como desventaja, cada proceso salta de p en p y no
aprovecha llaves contiguas, aunque aquí eso no importa porque cada llave genera su propia agenda
de subllaves. Además es un reparto estático: si un núcleo es más lento, todos lo esperan.

Su comunicación es mínima, solo el sondeo y el aviso. La pérdida que domina la pone el hardware:
con más núcleos activos baja el turbo, y tanto el i9 como el Apple Silicon mezclan núcleos de
rendimiento y de eficiencia.

## Bloque-cíclico

El bloque-cíclico parte el espacio en chunks de C = 2^c llaves, con `-c` igual a 16 por defecto.
El proceso r recorre los chunks r, r + p, r + 2p, y dentro de cada uno avanza en orden. Es el
punto medio entre los dos anteriores: con C = N/p es el naive y con C = 1 es el intercalado.

```text
para j = r, r + p, r + 2p, ... < N / C:
    para k = j·C .. (j + 1)·C - 1: probar k
```

Si j = ⌊k/C⌋ es el chunk de la llave, la halla el proceso j mod p tras ⌊j/p⌋·C + (k mod C) + 1
iteraciones. El speedup se acerca a p cuando k es mucho mayor que p·C. Con llaves diminutas se
parece al naive: la llave 123456, con C = 2^16, cae en el chunk 1, del proceso 1, en la posición
57 920, y da S_iter ≈ 2.13.

Este reparto combina el equilibrio del intercalado con el recorrido contiguo del naive, y el
tamaño de chunk permite moverse entre ambos. A cambio, agrega un parámetro, y con chunks grandes
respecto a la llave se pierde la consistencia. Su fuga principal es el desbalance dentro del
último chunk, de como mucho C llaves, más la misma comunicación mínima del intercalado.

## Maestro-trabajador

En este enfoque, el proceso 0 es el maestro y no busca: reparte chunks de C llaves a quien los
pide. Los procesos 1 a p − 1 son trabajadores, que piden un chunk, lo prueban y piden otro. Es el
único reparto dinámico, porque un núcleo más lento simplemente pide menos chunks.

```text
maestro:                                 trabajador:
  activos = p - 1                          pedir; recibir respuesta
  mientras activos > 0:                    mientras la respuesta sea WORK:
    recibir pedido (MPI_ANY_SOURCE)          pedir el siguiente (MPI_Send + MPI_Irecv)
    si es mensaje final (FOUND o DONE):      probar el chunk actual
      si FOUND: T_p = ahora; parar = 1         si halló k: enviar FOUND {k, iter}
      activos -= 1                                       MPI_Wait(respuesta) y terminar
    si no: responder STOP si parar o no      MPI_Wait(respuesta adelantada)
           quedan chunks; si no, WORK        enviar DONE
```

Dos invariantes evitan cuelgues. Cada pedido recibe exactamente una respuesta, y cada trabajador
termina con un único mensaje final, FOUND o DONE, que no espera respuesta. Como MPI no adelanta
mensajes del mismo origen con la misma etiqueta, cuando el maestro lee el mensaje final de un
trabajador ya atendió todos sus pedidos anteriores.

El trabajador pide el chunk siguiente antes de probar el actual. Ese doble buffer, con
`MPI_Send`, `MPI_Irecv` y `MPI_Wait`, esconde la ida y vuelta con el maestro detrás del cálculo.
La figura 9 muestra el intercambio de mensajes con tres procesos.

![Mensajes del maestro-trabajador con 3 procesos](img/maestro-trabajador-mensajes.png)

Figura 9. Mensajes del maestro-trabajador con un maestro y dos trabajadores.

Con p − 1 trabajadores que van al mismo ritmo, los chunks se reparten casi en round-robin entre
ellos. Por eso se comporta como el bloque-cíclico con p − 1 procesos: S_iter ≈ p − 1, unos 3
con `-np 4`, y su eficiencia ideal es (p − 1)/p, 0.75 con `-np 4`.

Su ventaja es el balanceo automático cuando los núcleos son desiguales o la máquina está
compartida. Además respeta el límite `-t` con una resolución de un chunk. Su desventaja es que
el maestro no busca, así que con pocos procesos se pierde 1/p del poder de cómputo. Con `-np 2`
hay un solo trabajador y no puede ganarle al secuencial.

Su fuga principal es una parte secuencial estructural, el maestro que solo reparte. Le sigue la
comunicación, un pedido y una respuesta por chunk, oculta en parte por el pedido adelantado.
Con más procesos, (p − 1)/p se acerca a 1.

## Híbrido MPI + OpenMP

El híbrido es el intercalado con dos niveles: p procesos MPI y T hilos de OpenMP por proceso,
según `OMP_NUM_THREADS`. El hilo t del proceso r es el trabajador global g = r·T + t, y prueba
g, g + pT, g + 2pT, y así sucesivamente. Es el mismo intercalado, ahora con w = p·T
trabajadores, como muestra la figura 10.

![Qué llaves prueba cada hilo del híbrido](img/hibrido-reparto.png)

Figura 10. Llaves que prueba cada hilo del híbrido con 16 llaves, p = 2 y T = 2.

```text
MPI_Init_thread(MPI_THREAD_FUNNELED)             # solo el hilo principal llama a MPI
mpi_search_setup; T = omp_get_max_threads() del proceso 0, MPI_Bcast(T)
antena: MPI_Irecv; MPI_Barrier; inicio = MPI_Wtime()
#pragma omp parallel num_threads(T) reduction(+: llaves)
    g = r·T + t;  workspace propio
    para k = g, g + pT, g + 2pT, ... < N:
        cada 4096 llaves:
            si t == 0 y MPI_Test(antena): stop = 1     # aviso de otro proceso
            si stop: salir
        si try_key(k): critical { guardar k, iteraciones }; stop = 1; salir
si un hilo la halló: MPI_Send(aviso) a todos        # fuera de la región paralela
MPI_Wait(antena); MPI_Reduce(llaves); reporte con hilos = T
```

La función `try_key` es reentrante, porque la agenda de subllaves de DES es local, así que basta
un `workspace` por hilo. La bandera de parada es un `atomic_int` de C11, y los hilos la leen cada
4096 llaves, igual que el sondeo MPI.

MPI se inicia con `MPI_THREAD_FUNNELED`. Solo el hilo 0, que es el principal, llama a
`MPI_Test`, y el aviso sale después de la región paralela. Así no hace falta que el MPI instalado
acepte llamadas desde varios hilos. El número de hilos lo decide el proceso 0 y lo difunde, para
que todos usen el mismo paso p·T.

Al medir hay un detalle importante. Open MPI 5 ata cada proceso a un solo núcleo por defecto, y
entonces los T hilos de un proceso se pelean ese núcleo y el híbrido no escala. En el i9, con 4
procesos, 1, 2 y 4 hilos tardaban casi lo mismo. Por eso el híbrido se lanza con
`mpirun --bind-to none`, como lo hacen `verificar.sh` y `campana.sh` (Open MPI Project, s. f.).
En macOS Open MPI no ata procesos, así que allá no cambia nada.

La llave la halla el hilo g = k mod pT, tras ⌊k/(pT)⌋ + 1 iteraciones. Entonces
S_iter = (k + 1) / (⌊k/(pT)⌋ + 1), que ronda p·T y nunca lo supera. Con T = 1 es exactamente el
intercalado, y para la eficiencia, el costo y la efectividad se usa w = p·T.

El híbrido usa todos los núcleos con pocos procesos MPI, lo que significa menos mensajes y menos
memoria que `intercalado -np p·T`, y conserva un speedup consistente para cualquier llave. A
cambio, agrega un nivel de sincronización y depende de cómo el lanzador ata los procesos. Si hay
más trabajadores que núcleos de rendimiento, los hilos que caen en núcleos de eficiencia frenan
al resto, porque el reparto es estático.

La consistencia con la versión sin OpenMP se demuestra con tres comparaciones de la campaña.
Con T = 1, el híbrido debe tardar lo mismo que el intercalado. Con T = 2 y 4, el tiempo debe
bajar de forma pareja en las tres llaves. Frente a `intercalado -np p·T`, con los mismos
trabajadores pero todos MPI, el tiempo debe ser parecido.

## Tabla teórica

La tabla da S_iter para cada llave con 56 bits, `-np 4` y C = 2^16.

| Llave | Naive | Intercalado | Bloque-cíclico | Maestro-trabajador |
|---|---|---|---|---|
| a · 123456 | 1 | 4.00 | 2.13 | 2.13 |
| b · 2^54 − 1 | 1 | 4 | 4 | 3 |
| c · 2^54 | 1.8·10^16 (aparente) | 4 | 4 | 3 |
| fácil · 2^55 + 1 | 1.8·10^16 (aparente) | 4 | 4 | 3 |
| media · 2^55 + 2^53 | 5 (aparente) | 4 | 4 | 3 |
| difícil · ⌈2^56/7 + 2^56/13⌉ | 1 | 4 | 4 | 3 |

Los valores del naive se sacan a mano con el bloque N/4 = 2^54. La llave media, k = N/2 + N/8,
cae en el bloque del proceso 2, que empieza en N/2 y avanza N/8 + 1 llaves. Su speedup es
(5N/8 + 1)/(N/8 + 1), es decir, 5.

La llave difícil vale cerca de 0.22·N, menos que N/4, así que cae en el bloque del proceso 0.
Ese proceso hace las mismas k + 1 iteraciones que el secuencial y el speedup es 1. La llave c,
N/4, es la primera del proceso 1, y la fácil, N/2 + 1, la segunda del proceso 2. En ambos casos
el speedup ronda N/4, cerca de 1.8·10^16.

Con 28 bits, que son los de las mediciones, la tabla es la misma salvo c y fácil en el naive,
que valen cerca de 6.7·10^7. Los presets escalan con N y cada llave conserva su posición
relativa. Por eso medir con menos bits es representativo.

## Consistencia frente a promedio

Si la llave fuera uniforme al azar entre 0 y N, el T_p esperado del naive y del intercalado
sería el mismo. En ambos, el proceso que la tiene recorre en promedio la mitad de N/p llaves:
E[T_p] = N/(2p), mientras que E[T_s] = N/2. El speedup esperado es p en los dos, y el S > p del
naive con c, fácil y media se compensa con las llaves que le dan S = 1.

Lo que gana un reparto alternativo es que el speedup ya no depende de dónde cae la llave. Da
cerca de p, o de p − 1, para cualquier llave, en vez de algo entre 1 y 10^16. Y si las llaves no
son uniformes, por ejemplo si son pequeñas, el naive da S = 1 y el intercalado S = p.
