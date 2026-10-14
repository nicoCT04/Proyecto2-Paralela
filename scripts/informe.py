#!/usr/bin/env python3
"""Arma docs/informe.pdf a partir de los Markdown de docs/.

Orden del informe (formato UVG): carátula, índice, introducción, antecedentes
(Parte A), desarrollo (Parte B y enfoques), resultados (análisis de speedup),
conclusiones, referencias y apéndices (Anexo 1 y Anexo 2).

Cada capítulo se convierte a HTML (paquete markdown) y Chromium sin interfaz lo
imprime a PDF. Se imprime dos veces: en la primera se ubica la página de cada
título por una marca invisible y en la segunda el índice ya lleva los números.
Al final se estampan los números de página (reportlab + pypdf), sin la carátula.

Requiere: chromium (o google-chrome) y python3 con markdown, pypdf y reportlab.
Uso: python3 scripts/informe.py [-o docs/informe.pdf]
"""
import argparse
import html
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

import markdown
from pypdf import PdfReader, PdfWriter
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas

DOCS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs")

# (archivo, parte del informe a la que pertenece)
CHAPTERS = [
    ("00-introduccion.md", "Introducción"),
    ("01-parte-a-des.md", "Antecedentes"),
    ("02-rutinas-y-mpi.md", "Antecedentes"),
    ("03-pruebas-b.md", "Desarrollo"),
    ("04-enfoques.md", "Desarrollo"),
    ("07-analisis-speedup.md", "Resultados"),
    ("08-conclusiones.md", "Conclusiones"),
    ("09-referencias.md", "Referencias"),
    ("05-anexo1-catalogo.md", "Apéndices"),
    ("06-anexo2-bitacora.md", "Apéndices"),
]

COVER = """
<section class="cover">
  <p>Universidad del Valle de Guatemala<br>Facultad de Ingeniería<br>
  Departamento de Ciencias de la Computación<br>CC3069 Computación Paralela y Distribuida<br>
  Ing. Juan Luis García</p>
  <p class="title">Proyecto 2<br>Fuerza bruta sobre DES con Open MPI</p>
  <table class="team">
    <tr><td>Esteban Cárcamo</td><td>23016</td></tr>
    <tr><td>Nicolás Concuá</td><td>23197</td></tr>
    <tr><td>Diego López</td><td>23747</td></tr>
  </table>
  <p>Repositorio: <a href="https://github.com/nicoCT04/Proyecto2-Paralela">https://github.com/nicoCT04/Proyecto2-Paralela</a></p>
  <p>Guatemala, 14 de octubre de 2026</p>
</section>
"""

CSS = """
@page { size: Letter; margin: 2.5cm; }
* { box-sizing: border-box; color: #000; }
body { font-family: Arial, "Liberation Sans", Helvetica, sans-serif; font-size: 11pt;
       line-height: 1.5; font-weight: normal; }
h1, h2, h3 { font-weight: bold; break-after: avoid; }
h1 { font-size: 16pt; text-align: center; margin: 0 0 18pt; }
h2 { font-size: 14pt; text-align: left; margin: 18pt 0 8pt; }
h3 { font-size: 12pt; text-align: left; margin: 14pt 0 6pt; }
p { text-align: justify; margin: 0 0 10pt; }
strong, b { font-weight: normal; }
code { font-family: "Courier New", "Liberation Mono", monospace; font-size: 10pt; }
pre { border: 0.5pt solid #000; padding: 6pt 8pt; margin: 0 0 10pt; font-size: 9pt;
      line-height: 1.3; white-space: pre-wrap; break-inside: avoid; }
pre code { font-size: inherit; }
table { border-collapse: collapse; width: 100%; margin: 4pt 0 12pt; font-size: 10pt;
        line-height: 1.3; }
th, td { border: 0.5pt solid #000; padding: 3pt 5pt; vertical-align: top; text-align: left; }
th { font-weight: normal; }
tr { break-inside: avoid; }
img { max-width: 100%; max-height: 18cm; display: block; margin: 6pt auto 4pt; }
p:has(> img) { break-after: avoid; margin: 0; }
p.caption { text-align: center; font-size: 10pt; margin: 2pt 0 14pt; }
sup, sub { font-size: 75%; line-height: 0; }
section.chapter { break-before: page; }
section.refs p { text-align: left; padding-left: 1.27cm; text-indent: -1.27cm; }
.mark { font-size: 1px; color: #fff; }
.cover { text-align: center; height: 22.5cm; display: flex; flex-direction: column;
         justify-content: space-between; }
.cover p { text-align: center; margin: 0; }
.cover .title { font-size: 16pt; font-weight: bold; }
.cover .team { width: auto; margin: 0 auto; font-size: 11pt; }
.cover .team td { border: none; padding: 1pt 12pt; }
.cover a { color: #000; text-decoration: none; }
.toc { break-before: page; }
.toc ol { list-style: none; padding: 0; margin: 0; }
.toc li { display: flex; margin: 0; line-height: 1.6; }
.toc li.l1 { font-weight: bold; margin-top: 6pt; }
.toc li.l2 { padding-left: 16pt; }
.toc li .dots { flex: 1; border-bottom: 1px dotted #000; margin: 0 4pt 5pt; }
.toc a { text-decoration: none; }
"""

HEADING = re.compile(r"<h([12])>(.*?)</h\1>", re.S)
SUPERSCRIPT = re.compile(r"\b([0-9A-Za-z.]+)\^([−-]?[0-9A-Za-z]+)")
SUBSCRIPT = re.compile(r"(?<![0-9A-Za-z_])([A-Za-zη])_(\{[^}]+\}|[0-9A-Za-z]+)")
CAPTION = re.compile(r"<p>(Figura \d+\..*?)</p>", re.S)


def typeset(body):
    """Exponentes y subíndices de verdad (2^56, T_s) fuera del código, y pies de figura."""
    parts = re.split(r"(<[^>]+>)", body)
    in_code = 0
    for i, part in enumerate(parts):
        if part.startswith("<"):
            if re.match(r"<(code|pre)\b", part):
                in_code += 1
            elif re.match(r"</(code|pre)>", part):
                in_code -= 1
            continue
        if in_code == 0:
            part = SUPERSCRIPT.sub(r"\1<sup>\2</sup>", part)
            parts[i] = SUBSCRIPT.sub(lambda m: f"{m.group(1)}<sub>{m.group(2).strip('{}')}</sub>", part)
    return CAPTION.sub(r'<p class="caption">\1</p>', "".join(parts))


def chapter_html(text, index):
    """Convierte un capítulo y le pone id y marca invisible a cada h1 y h2."""
    body = typeset(markdown.markdown(text, extensions=["tables", "fenced_code", "sane_lists"]))
    headings = []

    def tag(match):
        level, inner = int(match.group(1)), match.group(2)
        anchor = f"c{index}-{len(headings)}"
        label = re.sub(r"<[^>]+>", "", inner)
        headings.append((level, html.unescape(label).strip(), anchor))
        return (f'<h{level} id="{anchor}">{inner}<span class="mark"> QQ{anchor}QQ</span>'
                f"</h{level}>")

    return HEADING.sub(tag, body), headings


def build_html(pages_by_anchor):
    """HTML completo; el índice lleva números si ya se conocen las páginas."""
    sections, toc = [], []
    for index, (name, part) in enumerate(CHAPTERS):
        path = os.path.join(DOCS, name)
        if not os.path.exists(path):
            print(f"Aviso: falta docs/{name}; se omite.", file=sys.stderr)
            continue
        with open(path, encoding="utf-8") as handle:
            body, headings = chapter_html(handle.read(), index)
        css = "chapter refs" if name.startswith("09-") else "chapter"
        sections.append(f'<section class="{css}">{body}</section>')
        for level, label, anchor in headings:
            page = pages_by_anchor.get(anchor, "")
            toc.append(f'<li class="l{level}"><a href="#{anchor}">{html.escape(label)}</a>'
                       f'<span class="dots"></span><span>{page}</span></li>')
    base = "file://" + os.path.abspath(DOCS) + "/"
    return (f'<!doctype html><html lang="es"><head><meta charset="utf-8"><base href="{base}">'
            f"<title>Proyecto 2 · Fuerza bruta sobre DES</title><style>{CSS}</style></head><body>"
            f'{COVER}<section class="toc"><h1>Índice</h1><ol>{"".join(toc)}</ol></section>'
            f'{"".join(sections)}</body></html>')


def find_browser():
    for name in ("chromium", "chromium-browser", "google-chrome", "google-chrome-stable",
                 "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"):
        path = shutil.which(name) or (name if os.path.exists(name) else None)
        if path:
            return path
    sys.exit("Error: no se encontró chromium ni google-chrome.")


def print_pdf(browser, html_text, workdir, output):
    """Imprime el HTML a PDF con Chromium sin interfaz."""
    page = os.path.join(workdir, "informe.html")
    with open(page, "w", encoding="utf-8") as handle:
        handle.write(html_text)
    subprocess.run([browser, "--headless=new", "--disable-gpu", "--no-sandbox",
                    "--allow-file-access-from-files", "--no-pdf-header-footer",
                    f"--user-data-dir={os.path.join(workdir, 'perfil')}",
                    f"--print-to-pdf={output}", "file://" + page],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def locate_headings(pdf_path):
    """Página (contando desde 1) donde aparece la marca de cada título."""
    pages = {}
    for number, page in enumerate(PdfReader(pdf_path).pages, start=1):
        for anchor in re.findall(r"QQ(c\d+-\d+)QQ", page.extract_text() or ""):
            pages.setdefault(anchor, number)
    return pages


def stamp_page_numbers(source, output):
    """Numera todas las páginas menos la carátula, al pie y centrado."""
    reader = PdfReader(source)
    writer = PdfWriter()
    for number, page in enumerate(reader.pages, start=1):
        if number > 1:
            width, height = float(page.mediabox.width), float(page.mediabox.height)
            buffer = io.BytesIO()
            sheet = canvas.Canvas(buffer, pagesize=(width, height) if width else letter)
            sheet.setFont("Helvetica", 10)
            sheet.setFillGray(0)
            sheet.drawCentredString(width / 2, 36, str(number))
            sheet.save()
            buffer.seek(0)
            page.merge_page(PdfReader(buffer).pages[0])
        writer.add_page(page)
    writer.add_metadata({"/Title": "Proyecto 2 · Fuerza bruta sobre DES con Open MPI",
                         "/Author": "Esteban Cárcamo, Nicolás Concuá, Diego López"})
    with open(output, "wb") as handle:
        writer.write(handle)


def main():
    parser = argparse.ArgumentParser(description="Arma docs/informe.pdf desde docs/*.md.")
    parser.add_argument("-o", "--output", default=os.path.join(DOCS, "informe.pdf"))
    args = parser.parse_args()

    browser = find_browser()
    with tempfile.TemporaryDirectory() as workdir:
        draft = os.path.join(workdir, "borrador.pdf")
        print_pdf(browser, build_html({}), workdir, draft)
        pages = locate_headings(draft)
        final = os.path.join(workdir, "final.pdf")
        print_pdf(browser, build_html(pages), workdir, final)
        if locate_headings(final) != pages:
            print("Aviso: la paginación cambió entre pasadas; revise el índice.", file=sys.stderr)
        stamp_page_numbers(final, args.output)
    print(f"Informe guardado en {args.output} ({len(PdfReader(args.output).pages)} páginas)")


if __name__ == "__main__":
    main()
