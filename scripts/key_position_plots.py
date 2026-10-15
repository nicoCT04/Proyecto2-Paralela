#!/usr/bin/env python3
import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
IMG_DIR = os.path.join(ROOT, "docs", "img")
CAMPAIGN = os.path.join(ROOT, "resultados", "campana_escu_20261014-2308.csv")

BITS = 26
N = 1 << BITS
P = 4
THREADS = 2
BLOCK = N // P
CLASS_KEY = 60_000_000
COUNTER_KEY = 16_000_000
CLASS_MEASURED = {"naive": 5.11, "intercalado": 3.23}
MIN_KEY = 2_000_000

COLORS = {"secuencial": "#52514e", "naive": "#2a78d6", "intercalado": "#eb6834",
          "ciclico": "#1baf7a", "maestro_trabajador": "#eda100", "hibrido": "#e87ba4"}
LABELS = {"secuencial": "Secuencial", "naive": "Naive", "intercalado": "Intercalado",
          "ciclico": "Bloque-cíclico", "maestro_trabajador": "Maestro-trabajador",
          "hibrido": "Híbrido 4 × 2"}
INK = "#000000"
MUTED = "#52514e"
GRID = "#e4e3df"
NAIVE_ZONE = "#2a78d6"
INTER_ZONE = "#eb6834"
MILLION = 1e6


def style_axes(ax):
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(INK)
    ax.tick_params(colors=INK, labelsize=9)
    ax.yaxis.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def naive_tries(k):
    owner = min(k // BLOCK, P - 1)
    return k - owner * BLOCK + 1


def key_samples():
    keys = set(range(0, N, N // 2048))
    for owner in range(1, P):
        keys.update((owner * BLOCK - 1, owner * BLOCK))
    keys.update((N - 1, CLASS_KEY, COUNTER_KEY))
    return sorted(keys)


def naive_wins(k):
    return naive_tries(k) < k // P + 1


def zones():
    result = []
    start = 0
    current = naive_wins(0)
    step = N // 8192
    for k in range(step, N, step):
        state = naive_wins(k)
        if state != current:
            result.append((start, k, current))
            start, current = k, state
    result.append((start, N, current))
    return result


def plot_tries(path):
    keys = key_samples()
    x = [k / MILLION for k in keys]
    top = 18
    fig, ax = plt.subplots(figsize=(8.6, 4.6))
    for start, end, naive_better in zones():
        ax.axvspan(start / MILLION, end / MILLION, color=NAIVE_ZONE if naive_better else INTER_ZONE,
                   alpha=0.07, linewidth=0)
    for owner in range(1, P):
        ax.axvline(owner * BLOCK / MILLION, color=MUTED, linewidth=0.8, linestyle=":")
    for owner in range(P):
        ax.text((owner + 0.5) * BLOCK / MILLION, top * 0.985, f"bloque de P{owner}\n(naive)",
                ha="center", va="top", fontsize=7.5, color=MUTED)

    series = [
        ("secuencial", [(k + 1) / MILLION for k in keys], "-", 1.6),
        ("naive", [naive_tries(k) / MILLION for k in keys], "-", 2.2),
        ("maestro_trabajador", [(k / (P - 1) + 1) / MILLION for k in keys], "--", 1.6),
        ("intercalado", [(k // P + 1) / MILLION for k in keys], "-", 2.2),
        ("hibrido", [(k // (P * THREADS) + 1) / MILLION for k in keys], "-", 1.6),
    ]
    for name, y, style, width in series:
        label = LABELS[name]
        if name == "maestro_trabajador":
            label += " (≈ k/3)"
        if name == "hibrido":
            label += " (k/8)"
        if name == "intercalado":
            label += " y cíclico (k/4)"
        if name == "secuencial":
            label += " (k + 1)"
        ax.plot(x, y, style, color=COLORS[name], linewidth=width, label=label)

    for key, text, dy in ((CLASS_KEY, "k = 60 000 000\n(la de la clase)", -46),
                          (COUNTER_KEY, "k = 16 000 000", 40)):
        naive_y = naive_tries(key) / MILLION
        inter_y = (key // P + 1) / MILLION
        ax.plot([key / MILLION] * 2, [naive_y, inter_y], color=INK, linewidth=1)
        ax.plot([key / MILLION], [naive_y], "o", color=COLORS["naive"], markersize=6,
                markeredgecolor="white", markeredgewidth=1.2, zorder=5)
        ax.plot([key / MILLION], [inter_y], "o", color=COLORS["intercalado"], markersize=6,
                markeredgecolor="white", markeredgewidth=1.2, zorder=5)
        ax.annotate(text, (key / MILLION, (naive_y + inter_y) / 2), xytext=(-12 if key == CLASS_KEY else 10, dy),
                    textcoords="offset points", fontsize=8, color=INK,
                    ha="right" if key == CLASS_KEY else "left",
                    arrowprops={"arrowstyle": "-", "color": INK, "linewidth": 0.8})

    ax.set_xlim(0, N / MILLION)
    ax.set_ylim(0, top)
    ax.set_xlabel("Posición de la llave k (millones)  ·  2^26 llaves, 4 procesos", color=INK)
    ax.set_ylabel("Intentos del proceso que la halla (millones)", color=INK)
    style_axes(ax)
    handles, labels = ax.get_legend_handles_labels()
    handles += [plt.Rectangle((0, 0), 1, 1, color=INTER_ZONE, alpha=0.15),
                plt.Rectangle((0, 0), 1, 1, color=NAIVE_ZONE, alpha=0.15)]
    labels += ["zona donde gana el intercalado", "zona donde gana el naive"]
    ax.legend(handles, labels, frameon=False, fontsize=7.5, ncol=2, loc="upper center",
              bbox_to_anchor=(0.5, -0.14))
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def load_campaign():
    times = {}
    with open(CAMPAIGN, newline="") as handle:
        for row in csv.DictReader(handle):
            name = row["programa"]
            if name == "hibrido" and row["hilos"] != str(THREADS):
                continue
            times[(int(row["llave_real"]), name)] = float(row["tiempo_s"])
    return times


def plot_measured(path):
    times = load_campaign()
    keys = sorted({k for k, name in times if name == "secuencial" and k >= MIN_KEY})
    fig, ax = plt.subplots(figsize=(8.6, 4.4))
    for owner in range(1, P):
        ax.axvline(owner * BLOCK / MILLION, color=MUTED, linewidth=0.8, linestyle=":")
    ax.axhline(P, color=INK, linewidth=1, linestyle="--")
    ax.text(N / MILLION * 0.995, P + 0.12, "S = 4 (máximo con 4 procesos)", ha="right",
            va="bottom", fontsize=7.5, color=INK)
    ax.axhline(1, color=MUTED, linewidth=0.8, linestyle=":")
    ax.text(N / MILLION * 0.995, 1 + 0.1, "S = 1 (igual que el secuencial)", ha="right",
            va="bottom", fontsize=7.5, color=MUTED)
    markers = {"naive": "o", "intercalado": "s", "ciclico": "^", "maestro_trabajador": "D",
               "hibrido": "P"}
    for name in ("naive", "intercalado", "ciclico", "maestro_trabajador", "hibrido"):
        x = [k / MILLION for k in keys]
        y = [times[(k, "secuencial")] / times[(k, name)] for k in keys]
        ax.plot(x, y, markers[name], color=COLORS[name], markersize=6.5 if name == "naive" else 5.5,
                markeredgecolor="white", markeredgewidth=0.8, label=LABELS[name], linestyle="none",
                zorder=4 if name == "naive" else 3)
    for name, value in CLASS_MEASURED.items():
        ax.plot([CLASS_KEY / MILLION], [value], "*", color=COLORS[name], markersize=14,
                markeredgecolor=INK, markeredgewidth=0.8, zorder=6)
    ax.annotate("k = 60 000 000: naive 5.11 (★ azul)\ne intercalado 3.23 (★ naranja)",
                (CLASS_KEY / MILLION, CLASS_MEASURED["naive"]), xytext=(44, 4.75),
                textcoords="data", fontsize=8, color=INK, ha="center",
                arrowprops={"arrowstyle": "-", "color": INK, "linewidth": 0.8})
    for owner in range(P):
        ax.text((owner + 0.5) * BLOCK / MILLION, 8.35, f"bloque de P{owner} (naive)", ha="center",
                va="top", fontsize=7.5, color=MUTED)
    ax.set_xlim(0, N / MILLION)
    ax.set_ylim(0, 8.5)
    ax.set_xlabel("Posición de la llave k (millones)  ·  2^26 llaves, 4 procesos", color=INK)
    ax.set_ylabel("Speedup medido S = T_s / T_p", color=INK)
    style_axes(ax)
    ax.legend(frameon=False, fontsize=8, ncol=5, loc="upper center", bbox_to_anchor=(0.5, -0.14))
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    os.makedirs(IMG_DIR, exist_ok=True)
    plot_tries(os.path.join(IMG_DIR, "intentos_por_llave.png"))
    plot_measured(os.path.join(IMG_DIR, "speedup_llaves_azar.png"))
    print("Gráficas en docs/img/intentos_por_llave.png y docs/img/speedup_llaves_azar.png")


if __name__ == "__main__":
    sys.exit(main())
