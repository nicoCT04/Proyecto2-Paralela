#!/usr/bin/env bash
# Parte B.2: pruebas obligatorias del naive contra el secuencial.
#
#   a) llave 123456              b) llave 2^56/4 - 1          c) llave 2^56/4
#
# Con 56 bits las llaves b y c no terminan en el secuencial (y b tampoco en el
# naive), así que todo corre con límite de tiempo -t. Luego b y c se repiten
# con -b 28, donde sí terminan y se ve el comportamiento completo.
#
# Uso: [NP=4] [LIMIT=120] [MPIRUN=mpirun] scripts/pruebas_b2.sh
set -euo pipefail
cd "$(dirname "$0")/.."

NP=${NP:-4}
LIMIT=${LIMIT:-120}
MPIRUN=${MPIRUN:-mpirun}
OUT=resultados/pruebas_b2.csv

mkdir -p resultados
echo "programa,procesos,hilos,bits,llave_real,llave_hallada,proceso,iteraciones,llaves_totales,tiempo_s" > "$OUT"

# Corre un programa, deja la salida completa en pantalla y guarda su línea CSV.
run() {
    echo
    echo "\$ $*"
    "$@" | tee /dev/stderr | grep '^CSV,' | cut -d, -f2- >> "$OUT"
}

for key in a b c; do
    echo
    echo "##### Prueba $key · 56 bits · límite $LIMIT s"
    run ./bin/secuencial -f data/texto.txt -k "$key" -t "$LIMIT"
    run "$MPIRUN" -np "$NP" ./bin/naive -f data/texto.txt -k "$key" -t "$LIMIT"
done

for key in b c; do
    echo
    echo "##### Prueba $key · 28 bits (sí termina)"
    run ./bin/secuencial -f data/texto.txt -k "$key" -b 28
    run "$MPIRUN" -np "$NP" ./bin/naive -f data/texto.txt -k "$key" -b 28
done

echo
echo "Guardado en $OUT"
