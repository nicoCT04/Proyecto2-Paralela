#!/usr/bin/env bash
# Campaña de mediciones para el informe.
#
#   1. Comparación principal: secuencial, naive, intercalado, ciclico y
#      maestro_trabajador con -np $NP, para cada llave de $KEYS y cada
#      repetición. Si existe bin/hibrido, también con -np $NP y cada número de
#      hilos de $HYBRID_THREADS (por OMP_NUM_THREADS).
#   2. Barrido de procesos: intercalado y maestro_trabajador con la llave media
#      y cada -np de $NP_LIST, salvo $NP, que ya está en la comparación principal.
#
# Salida: resultados/campana_<host>_<AAAAMMDD-HHMM>.csv (caso, repetición y las
# columnas del CSV de los programas) y resultados/maquina_<host>.txt. De las
# repeticiones se reporta la mediana; eso lo calcula el análisis.
#
# Uso: [BITS=28] [REPS=5] [NP=4] [NP_LIST="2 4 8"] [KEYS="facil media dificil"]
#      [HYBRID_THREADS="1 2 4"] [MPIRUN=mpirun] scripts/campana.sh
set -euo pipefail
cd "$(dirname "$0")/.."

BITS=${BITS:-28}
REPS=${REPS:-5}
NP=${NP:-4}
NP_LIST=${NP_LIST:-"2 4 8"}
KEYS=${KEYS:-"facil media dificil"}
HYBRID_THREADS=${HYBRID_THREADS:-"1 2 4"}
MPIRUN=${MPIRUN:-mpirun}   # puede traer opciones, p. ej. "mpirun --oversubscribe"

TEXT=data/texto.txt
HOST=$(hostname -s)
OUT=resultados/campana_${HOST}_$(date +%Y%m%d-%H%M).csv
MACHINE=resultados/maquina_${HOST}.txt
HAS_HYBRID=0
[[ -x bin/hibrido ]] && HAS_HYBRID=1

mkdir -p resultados

# Ficha de la máquina: sin ella los tiempos no se pueden comparar.
{
    echo "Fecha: $(date)"
    uname -a
    if [[ $(uname -s) == Darwin ]]; then
        sysctl -n machdep.cpu.brand_string
        sysctl hw.physicalcpu hw.logicalcpu
        sysctl hw.perflevel0.physicalcpu hw.perflevel1.physicalcpu 2> /dev/null || true
    else
        lscpu
    fi
    $MPIRUN --version 2>&1 | head -n 1 || true
} > "$MACHINE"

# Calibración: 1 s de secuencial por llave da su valor real y el ritmo (llaves/s).
calibration=""
for key in $KEYS media; do
    line=$(./bin/secuencial -f "$TEXT" -k "$key" -b "$BITS" -t 1 | grep '^CSV,')
    calibration+="$key $(cut -d, -f6,10,11 <<< "$line" | tr , ' ')"$'\n'
done

# Estimación con el reparto ideal de cada programa (más ~0.5 s de arranque por corrida).
estimate=$(awk -v reps="$REPS" -v np="$NP" -v nplist="$NP_LIST" -v bits="$BITS" \
               -v keys="$KEYS" -v threads="$HYBRID_THREADS" -v hybrid="$HAS_HYBRID" '
    { real[$1] = $2; rate[$1] = $4 > 0 ? $3 / $4 : 1e9 }
    END {
        n = 2 ^ bits; c = 2 ^ 16; keys_total = 0; runs = 0
        nk = split(keys, ks, " "); nh = split(threads, hs, " ")
        for (i = 1; i <= nk; i++) {
            k = real[ks[i]]
            block = int(n / np); finder = int(k / block); if (finder > np - 1) finder = np - 1
            j = int(k / c)
            work = (k + 1) + (k - finder * block + 1) + (int(k / np) + 1) \
                   + (int(j / np) * c + (k % c) + 1) + (k + 1) / (np - 1)
            runs += 5
            if (hybrid) { for (h = 1; h <= nh; h++) work += (k + 1) / (np * hs[h]); runs += nh }
            keys_total += reps * work / rate[ks[i]]
        }
        k = real["media"]; np_count = split(nplist, ps, " ")
        for (i = 1; i <= np_count; i++) {
            p = ps[i]; if (p == np) continue
            work = (k + 1) / p; runs += 1
            if (p >= 2) { work += (k + 1) / (p - 1); runs += 1 }
            keys_total += reps * work / rate["media"]
        }
        printf "%.0f", keys_total + 0.5 * runs * reps
    }' <<< "$calibration")
echo "Campaña: BITS=$BITS REPS=$REPS NP=$NP NP_LIST=\"$NP_LIST\" KEYS=\"$KEYS\" hibrido=$HAS_HYBRID"
echo "Estimación aproximada: $((estimate / 60)) min ($estimate s). Salida: $OUT"

echo "caso,repeticion,programa,procesos,hilos,bits,llave_real,llave_hallada,proceso,iteraciones,llaves_totales,tiempo_s" > "$OUT"
failed=0

# run <caso> <repetición> <comando...>: guarda la línea CSV con caso y repetición.
run() {
    local case_name=$1 rep=$2
    shift 2
    local line
    line=$("$@" 2> /dev/null | grep '^CSV,' | cut -d, -f2-) || true
    if [[ -z $line ]]; then
        echo "FALLA (sin CSV): $*" >&2
        failed=$((failed + 1))
        return
    fi
    echo "$case_name,$rep,$line" | tee -a "$OUT"
}

for key in $KEYS; do
    for rep in $(seq 1 "$REPS"); do
        run "$key" "$rep" ./bin/secuencial -f "$TEXT" -k "$key" -b "$BITS"
        for program in naive intercalado ciclico maestro_trabajador; do
            run "$key" "$rep" $MPIRUN -np "$NP" "./bin/$program" -f "$TEXT" -k "$key" -b "$BITS"
        done
        if [[ $HAS_HYBRID -eq 1 ]]; then
            for threads in $HYBRID_THREADS; do
                export OMP_NUM_THREADS=$threads
                run "$key" "$rep" \
                    $MPIRUN -x OMP_NUM_THREADS -np "$NP" ./bin/hibrido -f "$TEXT" -k "$key" -b "$BITS"
            done
            unset OMP_NUM_THREADS
        fi
    done
done

for np in $NP_LIST; do
    [[ $np == "$NP" ]] && continue
    for rep in $(seq 1 "$REPS"); do
        run media "$rep" $MPIRUN -np "$np" ./bin/intercalado -f "$TEXT" -k media -b "$BITS"
        if [[ $np -ge 2 ]]; then
            run media "$rep" $MPIRUN -np "$np" ./bin/maestro_trabajador -f "$TEXT" -k media -b "$BITS"
        fi
    done
done

echo
echo "Guardado en $OUT y $MACHINE ($failed corridas sin CSV)"
[[ $failed -eq 0 ]]
