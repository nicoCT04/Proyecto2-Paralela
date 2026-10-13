# Parte A.1 y A.2: El algoritmo DES

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

Nuestro programa secuencial prueba unos 3.9·10^6 llaves por segundo en la Mac de Nico (ver
`resultados/secuencial_longitudes.csv`). Recorrer las 2^56 llaves le tomaría
7.2·10^16 / 3.9·10^6 ≈ 1.85·10^10 s, unos 585 años, y en promedio la mitad. Por eso el proyecto
paraleliza la búsqueda y, para medir, reduce el espacio con la opción `-b`.

## A.2 Diagrama de flujo

La figura 1 resume el cifrado de un bloque y la generación de las 16 subllaves. El diagrama se
generó con Mermaid desde `docs/img/des-flujo.mmd`.

![Diagrama de flujo de DES](img/des-flujo.png)

Figura 1. Cifrado de un bloque con DES (izquierda) y generación de las 16 subllaves (derecha).
