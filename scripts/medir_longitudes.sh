#!/usr/bin/env bash
# Parte A.3: tiempo de la búsqueda secuencial con distintas longitudes de llave.
#
# Para cada longitud de b bits se usa el peor caso, la llave 2^b - 1 (hay que
# probar las 2^b llaves), y se guarda una fila CSV por corrida.
#
# Uso: [OUT=archivo.csv] [REPS=n] scripts/medir_longitudes.sh [bits...]   (por defecto: 16 18 20 22 24 26)
set -euo pipefail
cd "$(dirname "$0")/.."

BITS=("$@")
[[ ${#BITS[@]} -eq 0 ]] && BITS=(16 18 20 22 24 26)
OUT=${OUT:-resultados/secuencial_longitudes.csv}
REPS=${REPS:-1}

mkdir -p resultados
echo "programa,procesos,hilos,bits,llave_real,llave_hallada,proceso,iteraciones,llaves_totales,tiempo_s" > "$OUT"
for bits in "${BITS[@]}"; do
    key=$(( (1 << bits) - 1 ))
    for (( rep = 1; rep <= REPS; rep++ )); do
        ./bin/secuencial -f data/texto.txt -k "$key" -b "$bits" | grep '^CSV,' | cut -d, -f2- | tee -a "$OUT"
    done
done
echo "Guardado en $OUT"
