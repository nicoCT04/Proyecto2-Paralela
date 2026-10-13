#!/usr/bin/env python3
"""Capturas de las corridas para el Anexo 2.

Corre de verdad cada comando de la lista, toma su salida (stdout y stderr) y
la guarda como imagen con aspecto de terminal en docs/capturas/anexo2-*.png,
con el prompt, el host y la fecha de la corrida. Así las capturas se rehacen
con un solo comando en la máquina donde se mide.

Requiere: chromium (o google-chrome) y Pillow. Corre lo que haya compilado en bin/.
Uso: [BITS=28] [NP=4] [THREADS=4] python3 scripts/capturas.py [nombre ...]
"""
import datetime
import html
import os
import shutil
import socket
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT_DIR = os.path.join(ROOT, "docs", "capturas")

BITS = os.environ.get("BITS", "28")
NP = os.environ.get("NP", "4")
THREADS = os.environ.get("THREADS", "4")
KEYS = ("facil", "media", "dificil")
TEXT = "-f data/texto.txt"
HYBRID = f"OMP_NUM_THREADS={THREADS} mpirun --bind-to none -x OMP_NUM_THREADS"


def per_key(command):
    return [command.format(key=key) for key in KEYS]


# (nombre del archivo, comandos)
CAPTURES = [
    ("secuencial", per_key(f"./bin/secuencial {TEXT} -k {{key}} -b {BITS}")),
    ("naive", per_key(f"mpirun -np {NP} ./bin/naive {TEXT} -k {{key}} -b {BITS}")),
    ("intercalado", per_key(f"mpirun -np {NP} ./bin/intercalado {TEXT} -k {{key}} -b {BITS}")),
    ("ciclico", per_key(f"mpirun -np {NP} ./bin/ciclico {TEXT} -k {{key}} -b {BITS}")),
    ("maestro_trabajador",
     per_key(f"mpirun -np {NP} ./bin/maestro_trabajador {TEXT} -k {{key}} -b {BITS}")),
    ("hibrido", per_key(f"{HYBRID} -np {NP} ./bin/hibrido {TEXT} -k {{key}} -b {BITS}")),
    ("barrido", [f"mpirun -np {p} ./bin/{program} {TEXT} -k media -b {BITS} | grep -E '^(===|Procesos|Encontrada|Tiempo|CSV)'"
                 for program in ("intercalado", "maestro_trabajador") for p in (2, 8)]),
    ("verificar", ["scripts/verificar.sh | tail -n 14"]),
]

PAGE = """<!doctype html><html><head><meta charset="utf-8"><style>
body {{ margin: 0; background: #1e1f22; }}
.term {{ font: 13px/1.38 "DejaVu Sans Mono", "Liberation Mono", Menlo, monospace; color: #d7dae0;
         padding: 14px 18px; white-space: pre-wrap; word-break: break-all; }}
.bar {{ background: #2b2d31; color: #9a9da3; font: 12px "DejaVu Sans", Arial, sans-serif;
        padding: 6px 12px; }}
.prompt {{ color: #7ec27e; }} .cmd {{ color: #ffffff; }} .csv {{ color: #e5c07b; }}
.err {{ color: #e06c75; }}
</style></head><body><div class="bar">{title}</div><div class="term">{body}</div></body></html>"""


def find_browser():
    for name in ("chromium", "chromium-browser", "google-chrome", "google-chrome-stable",
                 "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"):
        path = shutil.which(name) or (name if os.path.exists(name) else None)
        if path:
            return path
    sys.exit("Error: no se encontró chromium ni google-chrome.")


def run(command):
    """Corre un comando en la raíz del repo y devuelve su salida combinada."""
    result = subprocess.run(command, shell=True, cwd=ROOT, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    return result.stdout.rstrip("\n")


def render(browser, name, commands, workdir):
    """Corre los comandos y guarda la captura; devuelve la ruta del PNG."""
    host = socket.gethostname().split(".")[0]
    prompt = f"{os.environ.get('USER', 'user')}@{host}:~/Proyecto2-Paralela$"
    lines, rows = [], 0
    for command in commands:
        print(f"  $ {command}", flush=True)
        output = run(command)
        lines.append(f'<span class="prompt">{html.escape(prompt)}</span> '
                     f'<span class="cmd">{html.escape(command)}</span>')
        for line in output.splitlines():
            css = "csv" if line.startswith("CSV,") else "err" if line.startswith(("Error", "FALLA")) else ""
            lines.append(f'<span class="{css}">{html.escape(line)}</span>' if css else html.escape(line))
        rows += 2 + len(command) // 60 + len(output.splitlines()) + len(output) // 100
    stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    page = os.path.join(workdir, f"{name}.html")
    with open(page, "w", encoding="utf-8") as handle:
        handle.write(PAGE.format(title=html.escape(f"{host} · {stamp} · {name}"),
                                 body="\n".join(lines)))
    output = os.path.join(OUT_DIR, f"anexo2-{name}.png")
    height = 200 + rows * 24
    subprocess.run([browser, "--headless=new", "--disable-gpu", "--no-sandbox",
                    "--hide-scrollbars", f"--user-data-dir={os.path.join(workdir, 'perfil')}",
                    f"--window-size=1000,{height}", f"--screenshot={output}", "file://" + page],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    trim_background(output)
    return output


def trim_background(path):
    """La ventana se pide con altura de sobra; aquí se recorta el fondo vacío de abajo."""
    image = Image.open(path).convert("RGB")
    width, height = image.size
    background = image.getpixel((width - 2, height - 2))
    pixels = image.load()
    bottom = height
    while bottom > 1 and all(pixels[x, bottom - 1] == background for x in range(0, width, 2)):
        bottom -= 1
    image.crop((0, 0, width, min(height, bottom + 16))).save(path)


def main():
    wanted = set(sys.argv[1:])
    unknown = wanted - {name for name, _ in CAPTURES}
    if unknown:
        sys.exit(f"Error: capturas desconocidas: {', '.join(sorted(unknown))}")
    browser = find_browser()
    os.makedirs(OUT_DIR, exist_ok=True)
    with tempfile.TemporaryDirectory() as workdir:
        for name, commands in CAPTURES:
            if wanted and name not in wanted:
                continue
            print(f"Captura {name}:")
            print(f"  -> {render(browser, name, commands, workdir)}")


if __name__ == "__main__":
    main()
