#!/usr/bin/env bash
# Verifica que todos los programas hallen la llave real.
#
# Para cada programa, cada preset de llave (a, b, c, facil, media, dificil) y
# -np 2, 3 y 4 (el 3 prueba el residuo del reparto), compara llave_hallada con
# llave_real en la línea CSV. Con -b 20 todo es rápido; ciclico y
# maestro_trabajador usan -c 8 para ejercitar el reparto con muchos chunks.
# hibrido se prueba con 1 y 2 hilos por proceso (OMP_NUM_THREADS) y sin atar
# los procesos a un núcleo. Además revisa que -h salga con 0 y que
# maestro_trabajador con -np 1 dé error sin colgarse. Imprime OK o FALLA por
# caso y sale con código != 0 si algo falla.
#
# Uso: [BITS=20] [MPIRUN=mpirun] [LIMIT=60] scripts/verificar.sh
set -uo pipefail
cd "$(dirname "$0")/.."

BITS=${BITS:-20}
MPIRUN=${MPIRUN:-mpirun}
LIMIT=${LIMIT:-60}
KEYS=(a b c facil media dificil)
NPS=(2 3 4)
HYBRID_THREADS=(1 2)

MPI_PROGRAMS=("naive" "intercalado" "ciclico -c 8" "maestro_trabajador -c 8")

# Límite por caso para detectar cuelgues (macOS no trae timeout: se usa gtimeout si está).
# -k: prterun puede ignorar el SIGTERM si se traba al cerrar; 5 s después va SIGKILL.
if command -v timeout > /dev/null; then
    GUARD=(timeout -k 5 "$LIMIT")
elif command -v gtimeout > /dev/null; then
    GUARD=(gtimeout -k 5 "$LIMIT")
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
        # 124 (timeout) y 137 (SIGKILL de -k): un cuelgue no cuenta como error limpio.
        [[ $code -ne 0 && $code -ne 124 && $code -ne 137 ]]
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

# Open MPI ata cada proceso a un núcleo: sin --bind-to none los hilos se lo disputan.
if [[ -x bin/hibrido ]]; then
    for threads in "${HYBRID_THREADS[@]}"; do
        for np in "${NPS[@]}"; do
            for key in "${KEYS[@]}"; do
                OMP_NUM_THREADS=$threads check_key "hibrido T=$threads -np $np -k $key" \
                    "$MPIRUN" --bind-to none -x OMP_NUM_THREADS -np "$np" ./bin/hibrido \
                    -f data/texto.txt -k "$key" -b "$BITS"
            done
        done
    done
    check_exit "hibrido -h" 0 "$MPIRUN" -np 2 ./bin/hibrido -h
fi

# Con el chunk por defecto (2^16) y -b 18 solo hay 4 chunks: se acaban antes de
# hallar la llave y el protocolo cierra con STOP adelantados.
for np in 4 8; do
    for key in facil media dificil; do
        check_key "maestro_trabajador pocos chunks -np $np -k $key" \
            "$MPIRUN" -np "$np" ./bin/maestro_trabajador -f data/texto.txt -k "$key" -b 18
    done
done

check_exit "maestro_trabajador -np 1 da error" error \
    "$MPIRUN" -np 1 ./bin/maestro_trabajador -f data/texto.txt -k a -b "$BITS"

echo
echo "$total casos, $fails fallas"
[[ $fails -eq 0 ]]
