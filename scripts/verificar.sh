#!/usr/bin/env bash
# Verifica que todos los programas hallen la llave real.
#
# Para cada programa, cada preset de llave (a, b, c, facil, media, dificil) y
# -np 2, 3 y 4 (el 3 prueba el residuo del reparto), compara llave_hallada con
# llave_real en la línea CSV. Con -b 20 todo es rápido; ciclico y
# maestro_trabajador usan -c 8 para ejercitar el reparto con muchos chunks.
# Además revisa que -h salga con 0 y que maestro_trabajador con -np 1 dé error
# sin colgarse. Imprime OK o FALLA por caso y sale con código != 0 si algo falla.
#
# Uso: [BITS=20] [MPIRUN=mpirun] [LIMIT=60] scripts/verificar.sh
set -uo pipefail
cd "$(dirname "$0")/.."

BITS=${BITS:-20}
MPIRUN=${MPIRUN:-mpirun}
LIMIT=${LIMIT:-60}
KEYS=(a b c facil media dificil)
NPS=(2 3 4)

# Programas MPI y sus opciones extra; hibrido entra solo cuando ya existe.
MPI_PROGRAMS=("naive" "intercalado" "ciclico -c 8" "maestro_trabajador -c 8")
[[ -x bin/hibrido ]] && MPI_PROGRAMS+=("hibrido")

# Límite por caso para detectar cuelgues (macOS no trae timeout: se usa gtimeout si está).
if command -v timeout > /dev/null; then
    GUARD=(timeout "$LIMIT")
elif command -v gtimeout > /dev/null; then
    GUARD=(gtimeout "$LIMIT")
else
    GUARD=()
    echo "Aviso: sin timeout ni gtimeout; un cuelgue detendría el script." >&2
fi

total=0
fails=0

# Registra un caso como OK o FALLA.
report() {
    total=$((total + 1))
    if [[ $1 -eq 0 ]]; then
        echo "OK     $2"
    else
        echo "FALLA  $2"
        fails=$((fails + 1))
    fi
}

# check_key <etiqueta> <comando...>: el CSV debe traer llave_hallada == llave_real.
check_key() {
    local label=$1
    shift
    local line
    line=$(${GUARD[@]+"${GUARD[@]}"} "$@" 2> /dev/null | grep '^CSV,')
    [[ -n $line ]] && awk -F, '{ exit !($6 == $7) }' <<< "$line"
    report $? "$label"
}

# check_exit <etiqueta> <esperado: 0 | error> <comando...>: revisa el código de salida.
check_exit() {
    local label=$1 expected=$2
    shift 2
    ${GUARD[@]+"${GUARD[@]}"} "$@" > /dev/null 2>&1
    local code=$?
    if [[ $expected == 0 ]]; then
        [[ $code -eq 0 ]]
    else
        # 124 es el código de timeout: un cuelgue no cuenta como error limpio.
        [[ $code -ne 0 && $code -ne 124 ]]
    fi
    report $? "$label (salida $code)"
}

echo "##### Verificación con -b $BITS"
for key in "${KEYS[@]}"; do
    check_key "secuencial -k $key" ./bin/secuencial -f data/texto.txt -k "$key" -b "$BITS"
done

for entry in "${MPI_PROGRAMS[@]}"; do
    read -r -a parts <<< "$entry"
    program=${parts[0]}
    for np in "${NPS[@]}"; do
        for key in "${KEYS[@]}"; do
            check_key "$entry -np $np -k $key" \
                "$MPIRUN" -np "$np" "./bin/$program" -f data/texto.txt -k "$key" -b "$BITS" \
                "${parts[@]:1}"
        done
    done
    check_exit "$program -h" 0 "$MPIRUN" -np 2 "./bin/$program" -h
done

check_exit "maestro_trabajador -np 1 da error" error \
    "$MPIRUN" -np 1 ./bin/maestro_trabajador -f data/texto.txt -k a -b "$BITS"

echo
echo "$total casos, $fails fallas"
[[ $fails -eq 0 ]]
