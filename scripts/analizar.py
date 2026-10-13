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
procesos, Amdahl y Gustafson, y la consistencia del híbrido. Revisa que todas
las corridas hayan hallado la llave real y marca con REVISAR lo que rompe la
regla del proyecto: S > w fuera del naive (superlineal inesperado) o
S_rend > w en cualquier programa (rendimiento imposible). El S > w del naive es
aparente (efecto de búsqueda) y solo se etiqueta.

Con matplotlib (opcional) deja además en docs/img/ las gráficas de speedup por
llave, del barrido de procesos (con la ideal y Amdahl) y del híbrido.

Uso: python3 scripts/analizar.py resultados/campana_<host>_*.csv [-o salida.md] [--img dir]
Sale con código 1 si hay alguna alerta REVISAR.
"""
import argparse
import csv
import math
import os
import re
import statistics
import sys
from collections import defaultdict

CASE_ORDER = ["a", "b", "c", "facil", "media", "dificil"]
PROGRAM_ORDER = ["secuencial", "naive", "intercalado", "ciclico", "maestro_trabajador", "hibrido"]

# Paleta categórica en orden fijo (un color por programa, nunca reciclado).
COLORS = {"naive": "#2a78d6", "intercalado": "#eb6834", "ciclico": "#1baf7a",
          "maestro_trabajador": "#eda100", "hibrido": "#e87ba4"}
CASE_COLORS = {"facil": "#2a78d6", "media": "#eb6834", "dificil": "#1baf7a"}
INK = "#0b0b0b"
INK_MUTED = "#52514e"
GRID = "#e4e3df"

# Margen sobre w antes de alertar: el ruido de medición puede pasar p por poco.
TOLERANCE = 1.02


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


def amdahl_fp(speedup, p):
    """Fracción paralelizable despejada de Amdahl: f_p = (1 - 1/S) / (1 - 1/p)."""
    return (1 - 1 / speedup) / (1 - 1 / p)


def amdahl_speedup(fp, p):
    """Speedup de Amdahl con fracción paralelizable fp y p procesadores."""
    return 1 / ((1 - fp) + fp / p)


def gustafson_speedup(fs, p):
    """Speedup escalado de Gustafson: S = p - f_s (p - 1)."""
    return p - fs * (p - 1)


def ideal_speedup(group):
    """Speedup ideal del reparto: el maestro-trabajador tiene p - 1 trabajadores."""
    if group.program == "maestro_trabajador":
        return group.processes - 1
    return group.workers


def note(group, m, alerts):
    """Etiqueta de la fila según la regla del superlineal; las alertas se acumulan."""
    limit = group.workers * TOLERANCE
    notes = []
    if group.program == "naive" and m["S"] > limit:
        notes.append("aparente (búsqueda)")
    elif m["S"] > limit:
        notes.append("**REVISAR: superlineal inesperado**")
        alerts.append(f"superlineal inesperado: {label(group)} -np {group.processes}, "
                      f"llave {group.case}, S = {fmt(m['S'])} > w = {group.workers}")
    if m["S_rend"] > limit:
        notes.append("**REVISAR: rendimiento imposible**")
        alerts.append(f"rendimiento imposible: {label(group)} -np {group.processes}, "
                      f"llave {group.case}, S_rend = {fmt(m['S_rend'])} > w = {group.workers}")
    return ", ".join(notes)


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


def main_table(lines, groups, t_s, alerts):
    lines.append("| Programa | p | h | w | T_p mediana (mín–máx) s | S | E | C (s) | F | S_iter | S_rend | Nota |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|")
    for group in groups:
        m = metrics(group, t_s)
        lines.append(
            f"| {label(group)} | {group.processes} | {group.threads} | {group.workers} "
            f"| {time_cell(group)} | {fmt(m['S'])} | {fmt(m['E'])} | {fmt(m['C'])} "
            f"| {fmt(m['F'], 3)} | {fmt(m['S_iter'])} | {fmt(m['S_rend'])} | {note(group, m, alerts)} |")


def amdahl_section(lines, groups, seq, main_np, sweep_case):
    """f_p por Amdahl con -np main_np, techo 1/f_s y predicción frente al barrido."""
    lines.append("")
    lines.append("## Amdahl y Gustafson")
    lines.append("")
    lines.append(f"f_p = (1 − 1/S) / (1 − 1/p) con el S medido en -np {main_np}; f_s = 1 − f_p; "
                 "techo de Amdahl = 1/f_s. Esta «f_s» no es código secuencial (leer y cifrar "
                 "tardan milisegundos): es la pérdida efectiva por comunicación, sondeo y hardware "
                 "(turbo, núcleos de rendimiento y de eficiencia). En el maestro-trabajador incluye "
                 "además al maestro, que no busca. Gustafson (tiempo fijo): en el mismo tiempo se "
                 "prueban S veces más llaves, es decir, log2(S) bits más.")
    lines.append("")
    lines.append("| Programa | Llave | p | S | f_p | f_s | Techo 1/f_s | S Gustafson | Bits extra |")
    lines.append("|---|---|---|---|---|---|---|---|---|")
    fractions = {}
    for g in sorted(groups, key=lambda g: (program_rank(g.program), case_rank(g.case))):
        if g.program in ("secuencial", "naive", "hibrido") or g.processes != main_np:
            continue
        t_s = seq.get((g.case, g.bits))
        if t_s is None:
            continue
        speedup = t_s / g.time
        fp = amdahl_fp(speedup, g.processes)
        fs = 1 - fp
        if g.case == sweep_case:
            fractions[g.program] = fp
        ceiling = f"{1 / fs:.1f}" if 0 < fs else "∞"
        lines.append(f"| {g.program} | {g.case} | {g.processes} | {fmt(speedup)} | {fp:.3f} | "
                     f"{fs:.3f} | {ceiling} | {fmt(gustafson_speedup(fs, g.processes))} | "
                     f"{math.log2(speedup):.2f} |")

    sweep = [g for g in groups if g.case == sweep_case and g.program in fractions]
    swept = {program for program in fractions
             if len({g.processes for g in sweep if g.program == program}) > 1}
    sweep = [g for g in sweep if g.program in swept]
    if sweep:
        lines.append("")
        lines.append(f"Predicción de Amdahl para el barrido (llave {sweep_case}, f_p de -np {main_np}):")
        lines.append("")
        lines.append("| Programa | p | S medido | S Amdahl | Diferencia | Ideal del enfoque |")
        lines.append("|---|---|---|---|---|---|")
        for g in sorted(sweep, key=lambda g: (program_rank(g.program), g.processes)):
            measured = seq[(g.case, g.bits)] / g.time
            predicted = amdahl_speedup(fractions[g.program], g.processes)
            lines.append(f"| {g.program} | {g.processes} | {fmt(measured)} | {fmt(predicted)} | "
                         f"{(measured - predicted) / predicted * 100:+.1f} % | {ideal_speedup(g)} |")
    return fractions


def analyze(paths, output, img_dir):
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
    alerts_at = len(lines)
    lines.append("")
    alerts = []

    wrong = [g for g in all_groups if g.wrong_keys]
    lines.append("## Verificación de llaves")
    lines.append("")
    if wrong:
        for g in wrong:
            lines.append(f"- **REVISAR: llave equivocada** en {label(g)} -np {g.processes} "
                         f"({g.case}): {g.wrong_keys} corrida(s) no hallaron {g.real_key}.")
            alerts.append(f"llave equivocada: {label(g)} -np {g.processes}, llave {g.case}")
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
        main_table(lines, case_groups, t_s, alerts)

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
            main_table(lines, series, t_s, alerts)

    fractions = amdahl_section(lines, groups, seq, main_np, sweep_case)

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

    summary = ["", "**Alertas: ninguna.** Ningún enfoque alternativo pasa de S = w y ningún programa "
               "prueba llaves más rápido que w veces el secuencial."] if not alerts else \
              ["", f"**Alertas: {len(alerts)} REVISAR** (ver la regla del superlineal):", ""] + \
              [f"- REVISAR: {a}" for a in alerts]
    lines[alerts_at:alerts_at] = summary
    for alert in alerts:
        print(f"REVISAR: {alert}", file=sys.stderr)

    with open(output, "w") as handle:
        handle.write("\n".join(lines) + "\n")
    print(f"Análisis guardado en {output}")
    plot_all(groups, seq, fractions, main_np, sweep_case, host, img_dir)
    return alerts


def style_axes(ax):
    """Ejes discretos: rejilla horizontal tenue y texto en gris."""
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(INK_MUTED)
    ax.tick_params(colors=INK_MUTED, labelsize=9)
    ax.yaxis.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def plot_speedup_by_key(plt, groups, seq, main_np, path):
    """Barras de S por programa y llave, con la línea S = p; el naive aparente se recorta."""
    programs = [p for p in PROGRAM_ORDER[1:5] if any(g.program == p for g in groups)]
    cases = sorted({g.case for g in groups if g.program != "secuencial"}, key=case_rank)
    lookup = {(g.case, g.program): g for g in groups
              if g.processes == main_np and g.threads == 1}
    top = main_np * 1.6
    width = 0.8 / len(programs)
    fig, ax = plt.subplots(figsize=(8, 4.2))
    for i, program in enumerate(programs):
        for j, case in enumerate(cases):
            g = lookup.get((case, program))
            t_s = seq.get((case, g.bits)) if g else None
            if t_s is None:
                continue
            speedup = t_s / g.time
            x = j + (i - (len(programs) - 1) / 2) * width
            clipped = speedup > top
            ax.bar(x, min(speedup, top), width * 0.92, color=COLORS[program],
                   hatch="//" if clipped else None, edgecolor="white", linewidth=0.5,
                   label=program if j == 0 else None)
            text = f"{fmt(speedup)}\naparente" if clipped else fmt(speedup)
            ax.text(x, min(speedup, top) + 0.05, text, ha="center", va="bottom",
                    fontsize=7.5, color=INK)
    ax.axhline(main_np, color=INK_MUTED, linestyle="--", linewidth=1)
    ax.text(len(cases) - 0.5, main_np, f" S = p = {main_np}", va="bottom", ha="right",
            fontsize=8, color=INK_MUTED)
    ax.set_xticks(range(len(cases)), [f"llave {c}" for c in cases])
    ax.set_ylim(0, top * 1.15)
    ax.set_ylabel("Speedup S = T_s / T_p", color=INK)
    ax.set_title(f"Speedup por llave con -np {main_np}", color=INK, fontsize=11, loc="left")
    style_axes(ax)
    ax.legend(frameon=False, fontsize=8, ncol=len(programs), loc="upper center",
              bbox_to_anchor=(0.5, -0.1))
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_sweep(plt, groups, seq, fractions, sweep_case, path):
    """S contra p del barrido, con la ideal y la curva de Amdahl de cada programa."""
    fig, ax = plt.subplots(figsize=(7, 4.2))
    max_p = 1
    for program, offset in (("intercalado", 7), ("maestro_trabajador", -7)):
        series = sorted((g for g in groups if g.case == sweep_case and g.program == program),
                        key=lambda g: g.processes)
        if len(series) < 2:
            continue
        ps = [g.processes for g in series]
        max_p = max(max_p, ps[-1])
        speedups = [seq[(g.case, g.bits)] / g.time for g in series]
        ax.plot(ps, speedups, marker="o", markersize=6, linewidth=2, color=COLORS[program],
                label=f"{program} (medido)")
        ax.annotate(fmt(speedups[-1]), (ps[-1], speedups[-1]), textcoords="offset points",
                    xytext=(6, offset), va="center", fontsize=8, color=INK)
        if program in fractions:
            curve = [1 + i * (ps[-1] - 1) / 50 for i in range(51)]
            ax.plot(curve, [amdahl_speedup(fractions[program], p) for p in curve], linestyle=":",
                    linewidth=1.5, color=COLORS[program], label=f"{program} (Amdahl)")
    ax.plot([1, max_p], [1, max_p], linestyle="--", linewidth=1, color=INK_MUTED, label="ideal S = p")
    ax.set_xlabel("Procesos p", color=INK)
    ax.set_ylabel("Speedup S", color=INK)
    ax.set_title(f"Barrido de procesos (llave {sweep_case})", color=INK, fontsize=11, loc="left")
    style_axes(ax)
    ax.legend(frameon=False, fontsize=8)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_hybrid(plt, groups, seq, path):
    """Tiempo y speedup del híbrido contra los hilos por proceso, una línea por llave."""
    hybrid = [g for g in groups if g.program == "hibrido"]
    fig, (left, right) = plt.subplots(1, 2, figsize=(9, 3.8))
    max_t = 1
    for case in sorted({g.case for g in hybrid}, key=case_rank):
        series = sorted((g for g in hybrid if g.case == case), key=lambda g: g.threads)
        threads = [g.threads for g in series]
        max_t = max(max_t, threads[-1])
        color = CASE_COLORS.get(case, INK_MUTED)
        left.plot(threads, [g.time for g in series], marker="o", markersize=6, linewidth=2,
                  color=color, label=f"llave {case}")
        right.plot(threads, [seq[(g.case, g.bits)] / g.time for g in series], marker="o",
                   markersize=6, linewidth=2, color=color, label=f"llave {case}")
    processes = hybrid[0].processes
    right.plot([1, max_t], [processes, processes * max_t], linestyle="--", linewidth=1,
               color=INK_MUTED, label=f"ideal S = {processes}·h")
    left.set_ylabel("T_p (s)", color=INK)
    left.set_title(f"Híbrido -np {processes}: tiempo", color=INK, fontsize=11, loc="left")
    right.set_ylabel("Speedup S", color=INK)
    right.set_title(f"Híbrido -np {processes}: speedup", color=INK, fontsize=11, loc="left")
    for ax in (left, right):
        ax.set_xlabel("Hilos por proceso h", color=INK)
        ax.set_xticks(sorted({g.threads for g in hybrid}))
        style_axes(ax)
        ax.legend(frameon=False, fontsize=8)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_all(groups, seq, fractions, main_np, sweep_case, host, img_dir):
    """Genera las gráficas si matplotlib está instalado."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("Aviso: sin matplotlib; se omiten las gráficas.", file=sys.stderr)
        return []
    os.makedirs(img_dir, exist_ok=True)
    paths = [os.path.join(img_dir, f"speedup_llaves_{host}.png")]
    plot_speedup_by_key(plt, groups, seq, main_np, paths[-1])
    if any(len({g.processes for g in groups if g.case == sweep_case and g.program == p}) > 1
           for p in ("intercalado", "maestro_trabajador")):
        paths.append(os.path.join(img_dir, f"barrido_{host}.png"))
        plot_sweep(plt, groups, seq, fractions, sweep_case, paths[-1])
    if any(g.program == "hibrido" for g in groups):
        paths.append(os.path.join(img_dir, f"hibrido_{host}.png"))
        plot_hybrid(plt, groups, seq, paths[-1])
    for path in paths:
        print(f"Gráfica guardada en {path}")
    return paths


def parse_args():
    parser = argparse.ArgumentParser(description="Análisis de la campaña de mediciones.")
    parser.add_argument("csv", nargs="+", help="resultados/campana_<host>_*.csv")
    parser.add_argument("-o", "--output", help="archivo .md de salida "
                        "(por defecto resultados/analisis_<host>.md)")
    parser.add_argument("--img", default=os.path.join("docs", "img"),
                        help="carpeta de las gráficas (por defecto docs/img)")
    return parser.parse_args()


def main():
    args = parse_args()
    output = args.output or os.path.join("resultados", f"analisis_{host_from_paths(args.csv)}.md")
    alerts = analyze(args.csv, output, args.img)
    sys.exit(1 if alerts else 0)


if __name__ == "__main__":
    main()
