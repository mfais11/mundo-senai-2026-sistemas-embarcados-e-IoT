#!/usr/bin/env python3
"""Gera firmware/*/pagina.h a partir de web/index.html.

Rode sempre que alterar a página:  python3 tools/gerar_pagina.py
"""
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
SKETCHES = ["maquete_iot", "maquete_iot_alexa"]
DELIM = "PAGINA"

html = (RAIZ / "web" / "index.html").read_text(encoding="utf-8")
if f'){DELIM}"' in html:
    raise SystemExit(f'web/index.html contém ){DELIM}", troque o delimitador em {__file__}')

conteudo = (
    "// Gerado por tools/gerar_pagina.py a partir de web/index.html. Não edite à mão.\n"
    "#pragma once\n"
    "#include <Arduino.h>\n\n"
    f'const char PAGINA_HTML[] PROGMEM = R"{DELIM}({html}){DELIM}";\n'
)

for sketch in SKETCHES:
    destino = RAIZ / "firmware" / sketch / "pagina.h"
    destino.write_text(conteudo, encoding="utf-8")
    print(f"{destino.relative_to(RAIZ)}  ({len(html.encode()) / 1024:.1f} KB)")
