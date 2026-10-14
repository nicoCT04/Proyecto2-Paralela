# Parte A.1 a A.3: DES y la búsqueda secuencial

## A.1 ¿Qué es DES?

El *Data Encryption Standard* (DES) es un algoritmo de cifrado simétrico por bloques. La misma
llave sirve para cifrar y para descifrar. Se publicó como estándar federal de Estados Unidos en
1977, a partir del cifrado *Lucifer* de IBM, y su última revisión es la FIPS PUB 46-3 de 1999
(NIST, 1999). El NIST lo retiró en 2005 porque su llave es demasiado corta para la capacidad de
cómputo actual (NIST, 2005).

| Característica | Valor |
|---|---|
| Tamaño de bloque | 64 bits (8 bytes) |
| Tamaño de llave | 64 bits, de los cuales 8 son de paridad: 56 bits efectivos |
| Espacio de llaves | 2^56 = 72 057 594 037 927 936 ≈ 7.2·10^16 |
| Estructura | Red de Feistel de 16 rondas |
| Subllaves | 16 subllaves de 48 bits, una por ronda |

El último bit de cada uno de los 8 bytes de la llave es de paridad impar y no interviene en el
cifrado. Por eso, aunque la llave ocupa 64 bits, la fuerza bruta solo tiene que recorrer 2^56
combinaciones.

### Cifrado de un bloque

El bloque de 64 bits pasa primero por una permutación inicial (IP), que reordena sus bits según
una tabla fija. El resultado se parte en dos mitades de 32 bits, L₀ a la izquierda y R₀ a la
derecha.

Luego vienen 16 rondas de Feistel. En la ronda i, la nueva mitad izquierda es la derecha
anterior, L_i = R_{i−1}, y la nueva derecha es R_i = L_{i−1} ⊕ f(R_{i−1}, K_i). Solo la mitad
derecha pasa por la función f; la izquierda solo se combina con un XOR.

Tras la ronda 16 se intercambian las mitades y el bloque queda R₁₆ ‖ L₁₆. Por último, la
permutación final IP⁻¹, inversa de la inicial, entrega el bloque cifrado de 64 bits.

### La función f

La función f(R, K) recibe los 32 bits de la mitad derecha y los 48 bits de la subllave de la
ronda (NIST, 1999; Stallings, 2020). Primero, la expansión E lleva la mitad derecha de 32 a 48
bits duplicando 16 de ellos. Después se combina con la subllave mediante un XOR.

Los 48 bits resultantes se parten en 8 grupos de 6, y cada grupo entra a una caja S. Cada caja
convierte 6 bits en 4 con una tabla: los bits de los extremos eligen la fila y los 4 centrales,
la columna. Las cajas S son la única parte no lineal de DES y la fuente de su seguridad. Los 32
bits que salen se reordenan con la permutación P.

### Generación de subllaves

La permutación PC-1 toma la llave de 64 bits, descarta los 8 bits de paridad y reordena los 56
restantes. Esos 56 bits se dividen en dos mitades de 28, C₀ y D₀.

En cada ronda, C y D rotan a la izquierda una posición en las rondas 1, 2, 9 y 16, y dos
posiciones en las demás. En total suman 28, así que al final vuelven a su estado inicial. De
los 56 bits de C_i ‖ D_i, la permutación PC-2 elige 48, que forman la subllave K_i.

### Descifrado

Gracias a la estructura de Feistel, descifrar es el mismo algoritmo con las subllaves en orden
inverso, de K₁₆ a K₁. La función f no tiene que ser invertible, porque cada ronda se deshace
aplicando otra vez el mismo XOR (Stallings, 2020).

### Modo ECB y relleno

DES cifra bloques de 8 bytes. Para textos más largos usamos el modo ECB (*Electronic
Codebook*), que cifra cada bloque por separado con la misma llave. Nuestro texto, «Esta es una
prueba de proyecto 2», mide 32 bytes, es decir, 4 bloques exactos. Si no fuera múltiplo de 8,
el último bloque se rellenaría con ceros (función `padded_length`).

ECB no es seguro para mensajes reales. Dos bloques de texto iguales producen dos bloques
cifrados iguales, y así se filtran patrones del mensaje. Los modos encadenados como CBC o CTR
lo evitan (Stallings, 2020). En este proyecto no importa, porque lo que se ataca es la llave.

### DES ante la fuerza bruta

Con 56 bits efectivos, DES quedó al alcance del *hardware* dedicado a finales de los noventa.
En 1998, la máquina Deep Crack de la Electronic Frontier Foundation, que costó unos 250 000
dólares, recuperó una llave en 56 horas. En enero de 1999, el DES Challenge III, resuelto por
la EFF junto con distributed.net, lo logró en 22 horas y 15 minutos (EFF, 1998).

Nuestro programa secuencial prueba unos 3·10^6 llaves por segundo en la laptop de Esteban, la
máquina de las mediciones (sección A.3). A ese ritmo, recorrer las 2^56 llaves le tomaría unos
770 años, y en promedio la mitad. Por eso el proyecto paraleliza la búsqueda y, para medir,
reduce el espacio con la opción `-b`.

## A.2 Diagrama de flujo

La figura 1 resume el cifrado de un bloque y la generación de las 16 subllaves. El diagrama se
generó con Mermaid desde `docs/img/des-flujo.mmd`.

![Diagrama de flujo de DES](img/des-flujo.png)

Figura 1. Cifrado de un bloque con DES (izquierda) y generación de las 16 subllaves (derecha).

## A.3 Búsqueda secuencial con distintas longitudes de llave

El programa `secuencial` prueba las llaves 0, 1, 2 y así en orden, hasta que el texto descifrado
contiene la frase clave. Para medir cómo crece el tiempo con la longitud de la llave se usó el
peor caso de cada longitud: con b bits, la llave 2^b − 1, que obliga a probar las 2^b llaves.

Las corridas las hace el script `medir_longitudes.sh`. En la laptop de Esteban cada longitud se
repitió 5 veces y se reporta la mediana. En la Mac de Nico hubo una sola corrida por longitud, que
sirve como comparación. Los datos crudos están en la carpeta `resultados/`, en los archivos
`secuencial_longitudes_escu.csv` (laptop) y `secuencial_longitudes.csv` (Mac).

| Bits | Llaves probadas | Laptop de Esteban (s) | Ritmo (llaves/s) | Mac de Nico (s) |
|---|---|---|---|---|
| 16 | 65 536 | 0.044 | 1.50·10^6 | 0.020 |
| 18 | 262 144 | 0.112 | 2.35·10^6 | 0.071 |
| 20 | 1 048 576 | 0.370 | 2.83·10^6 | 0.272 |
| 22 | 4 194 304 | 1.370 | 3.06·10^6 | 1.082 |
| 24 | 16 777 216 | 5.437 | 3.09·10^6 | 4.322 |
| 26 | 67 108 864 | 21.939 | 3.06·10^6 | 17.241 |

El tiempo crece de forma exponencial con la longitud de la llave. Cada bit duplica el espacio,
así que cada 2 bits el tiempo se multiplica por 4: de 22 a 24 bits pasa de 1.37 s a 5.44 s, y de
24 a 26 bits, a 21.94 s. Dicho de otro modo, el tiempo es lineal en el número de llaves, porque
cada intento cuesta lo mismo.

Con 16 y 18 bits el ritmo es menor. Esas corridas duran milisegundos, y en ese tiempo la CPU
todavía no alcanza su frecuencia máxima. Desde 22 bits el ritmo se estabiliza en unos
3.1·10^6 llaves por segundo, y ese es el valor que sirve para extrapolar.

Con ese ritmo, 28 bits tomarían unos 87 s en el peor caso, y la llave media de la campaña, que
está en el 62.5 % del espacio, unos 54 s; la campaña midió 56.6 s. Con 56 bits, el peor caso
tomaría 2^56 / 3.1·10^6 ≈ 2.3·10^10 s, más de 700 años. La Mac es alrededor de un 27 % más
rápida, pero eso no cambia el orden de magnitud. Por eso las mediciones del proyecto se hacen con
28 bits.
