#!/usr/bin/env python3
"""Análisis de la campaña de mediciones.

Lee uno o más resultados/campana_<host>_*.csv (de scripts/campana.sh), agrupa
por (caso, programa, procesos, hilos, bits), toma la mediana de las
repeticiones y calcula, con T_s = mediana del secuencial en el mismo caso y los
mismos bits y w = procesos · hilos trabajadores:

    S = T_s / T_p            E = S / w            C = w · T_p        F = S / C
    S_iter = (llave_real + 1) / iteraciones
    S_rend = (llaves_totales / T_p) / ((llave_real + 1) / T_s)

Deja resultados/analisis_<host>.md con una tabla por llave, el barrido de
procesos y la consistencia del híbrido, y revisa que todas las corridas hayan
hallado la llave real.

Uso: python3 scripts/analizar.py resultados/campana_<host>_*.csv [-o salida.md]
"""
import argparse
import csv
import os
import re
import statistics
import sys
from collections import defaultdict

CASE_ORDER = ["a", "b", "c", "facil", "media", "dificil"]
PROGRAM_ORDER = ["secuencial", "naive", "intercalado", "ciclico", "maestro_trabajador", "hibrido"]


class Group:
    """Repeticiones de un mismo caso, programa, procesos, hilos y bits."""

    def __init__(self, case, program, processes, threads, bits, real_key):
        self.case = case
        self.program = program
        self.processes = processes
        self.threads = threads
        self.bits = bits
        self.real_key = real_key
        self.times = []
        self.iterations = []
        self.total_keys = []
        self.wrong_keys = 0

    @property
    def workers(self):
        return self.processes * self.threads

    @property
    def time(self):
        return statistics.median(self.times)

    @property
    def keys_tried(self):
        return statistics.median(self.total_keys)

    @property
    def finder_iterations(self):
        return statistics.median(self.iterations)


def case_rank(case):
    return CASE_ORDER.index(case) if case in CASE_ORDER else len(CASE_ORDER)


def program_rank(program):
    return PROGRAM_ORDER.index(program) if program in PROGRAM_ORDER else len(PROGRAM_ORDER)


def host_from_paths(paths):
    """Toma el host del nombre campana_<host>_<fecha>.csv del primer archivo."""
    match = re.match(r"campana_(.+?)(?:_\d{8}-\d{4})?\.csv$", os.path.basename(paths[0]))
    return match.group(1) if match else "campana"


def load_groups(paths):
    """Lee los CSV y agrupa sus filas; también devuelve cuántas filas se leyeron."""
    groups = {}
    rows = 0
    for path in paths:
        with open(path, newline="") as handle:
            for row in csv.DictReader(handle):
                rows += 1
                key = (row["caso"], row["programa"], int(row["procesos"]), int(row["hilos"]),
                       int(row["bits"]))
                group = groups.get(key)
                if group is None:
                    group = Group(*key, int(row["llave_real"]))
                    groups[key] = group
                if row["llave_hallada"] != row["llave_real"]:
                    group.wrong_keys += 1
                    continue
                group.times.append(float(row["tiempo_s"]))
                group.iterations.append(int(row["iteraciones"]))
                group.total_keys.append(int(row["llaves_totales"]))
    return [g for g in groups.values() if g.times], groups.values(), rows


def sequential_times(groups):
    """T_s por (caso, bits): mediana del programa secuencial."""
    return {(g.case, g.bits): g.time for g in groups if g.program == "secuencial"}


def metrics(group, t_s):
    """Speedup, eficiencia, costo, efectividad y los dos speedups de control."""
    t_p = group.time
    speedup = t_s / t_p
    cost = group.workers * t_p
    work = group.real_key + 1
    return {
        "S": speedup,
        "E": speedup / group.workers,
        "C": cost,
        "F": speedup / cost,
        "S_iter": work / group.finder_iterations,
        "S_rend": (group.keys_tried / t_p) / (work / t_s),
    }


def fmt(value, digits=2):
    """Número legible: notación científica si es enorme o diminuto."""
    if value != 0 and (abs(value) >= 1e5 or abs(value) < 10 ** -digits):
        mantissa, exponent = f"{value:.2e}".split("e")
        return f"{mantissa}·10^{int(exponent)}"
    return f"{value:.{digits}f}"


def time_cell(group):
    if len(group.times) == 1:
        return f"{group.time:.3f}"
    return f"{group.time:.3f} ({min(group.times):.3f}–{max(group.times):.3f})"


def label(group):
    if group.program == "hibrido":
        return f"hibrido ({group.threads} hilos)"
    return group.program


def main_table(lines, groups, t_s):
    lines.append("| Programa | p | h | w | T_p mediana (mín–máx) s | S | E | C (s) | F | S_iter | S_rend |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|")
    for group in groups:
        m = metrics(group, t_s)
        lines.append(
            f"| {label(group)} | {group.processes} | {group.threads} | {group.workers} "
            f"| {time_cell(group)} | {fmt(m['S'])} | {fmt(m['E'])} | {fmt(m['C'])} "
            f"| {fmt(m['F'], 3)} | {fmt(m['S_iter'])} | {fmt(m['S_rend'])} |")


def analyze(paths, output):
    groups, all_groups, rows = load_groups(paths)
    if not groups:
        sys.exit("Error: no hay corridas válidas en los archivos dados.")
    seq = sequential_times(groups)
    host = host_from_paths(paths)
    bits = sorted({g.bits for g in groups})
    reps = max(len(g.times) for g in groups)
    main_np = max((g.processes for g in groups
                   if g.program == "naive"), default=4)

    lines = [f"# Análisis de la campaña · {host}", ""]
    lines.append(f"- Archivos: {', '.join(os.path.basename(p) for p in paths)}")
    lines.append(f"- Corridas: {rows} · bits: {', '.join(map(str, bits))} · "
                 f"repeticiones por caso: hasta {reps} (se usa la **mediana**)")
    lines.append(f"- w = procesos · hilos; T_s = mediana del `secuencial` con la misma llave y bits.")
    lines.append("")

    wrong = [g for g in all_groups if g.wrong_keys]
    lines.append("## Verificación de llaves")
    lines.append("")
    if wrong:
        for g in wrong:
            lines.append(f"- **REVISAR: llave equivocada** en {label(g)} -np {g.processes} "
                         f"({g.case}): {g.wrong_keys} corrida(s) no hallaron {g.real_key}.")
    else:
        lines.append(f"Las {rows} corridas hallaron la llave real (`llave_hallada == llave_real`).")
    lines.append("")

    by_case = defaultdict(list)
    for g in groups:
        by_case[(g.case, g.bits)].append(g)

    lines.append(f"## Comparación principal (-np {main_np})")
    for (case, case_bits) in sorted(by_case, key=lambda c: (case_rank(c[0]), c[1])):
        t_s = seq.get((case, case_bits))
        if t_s is None:
            continue
        case_groups = [g for g in by_case[(case, case_bits)]
                       if g.program == "secuencial" or g.processes == main_np]
        case_groups.sort(key=lambda g: (program_rank(g.program), g.threads))
        real_key = case_groups[0].real_key
        lines.append("")
        lines.append(f"### Llave {case} · k = {real_key} · {case_bits} bits · T_s = {t_s:.3f} s")
        lines.append("")
        main_table(lines, case_groups, t_s)

    sweep_case = "media"
    sweep = [g for g in groups if g.case == sweep_case and g.program in ("intercalado", "maestro_trabajador")]
    if len({g.processes for g in sweep}) > 1:
        lines.append("")
        lines.append(f"## Barrido de procesos (llave {sweep_case})")
        for program in ("intercalado", "maestro_trabajador"):
            series = sorted((g for g in sweep if g.program == program), key=lambda g: g.processes)
            if not series:
                continue
            t_s = seq[(sweep_case, series[0].bits)]
            lines.append("")
            lines.append(f"### {program} · T_s = {t_s:.3f} s")
            lines.append("")
            main_table(lines, series, t_s)

    hybrid = [g for g in groups if g.program == "hibrido"]
    if hybrid:
        lines.append("")
        lines.append("## Consistencia del híbrido")
        lines.append("")
        lines.append("Con T = 1 el híbrido es el intercalado; con más hilos debe bajar de forma pareja "
                     "en todas las llaves, y frente a `intercalado -np w` (los mismos trabajadores, "
                     "todos MPI) debe quedar parecido.")
        lines.append("")
        lines.append("| Llave | Hilos | w | T_p híbrido (s) | S híbrido | T_p intercalado -np w (s) | Híbrido / intercalado |")
        lines.append("|---|---|---|---|---|---|---|")
        interleaved = {(g.case, g.bits, g.processes): g for g in groups if g.program == "intercalado"}
        for g in sorted(hybrid, key=lambda g: (case_rank(g.case), g.threads)):
            t_s = seq.get((g.case, g.bits))
            if t_s is None:
                continue
            twin = interleaved.get((g.case, g.bits, g.workers))
            twin_time = f"{twin.time:.3f}" if twin else "—"
            ratio = f"{g.time / twin.time:.2f}" if twin else "—"
            lines.append(f"| {g.case} | {g.threads} | {g.workers} | {g.time:.3f} | "
                         f"{fmt(t_s / g.time)} | {twin_time} | {ratio} |")

    with open(output, "w") as handle:
        handle.write("\n".join(lines) + "\n")
    print(f"Análisis guardado en {output}")
    return host


def parse_args():
    parser = argparse.ArgumentParser(description="Análisis de la campaña de mediciones.")
    parser.add_argument("csv", nargs="+", help="resultados/campana_<host>_*.csv")
    parser.add_argument("-o", "--output", help="archivo .md de salida "
                        "(por defecto resultados/analisis_<host>.md)")
    return parser.parse_args()


def main():
    args = parse_args()
    output = args.output or os.path.join("resultados", f"analisis_{host_from_paths(args.csv)}.md")
    analyze(args.csv, output)


if __name__ == "__main__":
    main()
