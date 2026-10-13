#!/usr/bin/env python3
"""Empaqueta la interfaz web y las maquetas de ejemplo dentro del firmware.

Genera:
  src/web_assets.h      index.html (con los .js insertados, comprimido gzip),
                        manifest.json e iconos
  src/default_layouts.h maquetas de ejemplo de firmware/layouts/*.json

Uso:
  python3 firmware/tools/build_web.py           regenerar
  python3 firmware/tools/build_web.py --check   comprobar que están al día (CI)

Tras editar algo en firmware/web/ o firmware/layouts/ hay que volver a
ejecutarlo (PlatformIO lo hace solo antes de compilar, ver platformio.ini).
"""
import gzip
import hashlib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEB = ROOT / "web"
LAYOUTS = ROOT / "layouts"
OUT_ASSETS = ROOT / "src" / "web_assets.h"
OUT_LAYOUTS = ROOT / "src" / "default_layouts.h"


def inline_html():
    html = (WEB / "index.html").read_text(encoding="utf-8")

    def repl(m):
        js = (WEB / m.group(1)).read_text(encoding="utf-8")
        if "</script" in js:
            raise SystemExit(f"{m.group(1)} contiene '</script'")
        return "<script>\n" + js + "\n</script>"

    return re.sub(r'<script src="([\w.-]+)"></script>', repl, html)


def c_bytes(name, data):
    lines = []
    for i in range(0, len(data), 20):
        lines.append("  " + ",".join(f"0x{b:02x}" for b in data[i:i + 20]) + ",")
    return f"static const uint8_t {name}[] PROGMEM = {{\n" + "\n".join(lines) + "\n};\n"


def c_text(name, text):
    if ')RAW"' in text:
        raise SystemExit(f"{name}: el texto contiene el delimitador )RAW\"")
    return f'static const char {name}[] PROGMEM = R"RAW({text})RAW";\n'


def source_hash(parts):
    h = hashlib.sha256()
    for p in parts:
        h.update(p if isinstance(p, bytes) else p.encode("utf-8"))
    return h.hexdigest()[:16]


def build():
    html = inline_html()
    manifest = (WEB / "manifest.json").read_text(encoding="utf-8")
    svg = (WEB / "icon.svg").read_text(encoding="utf-8").strip()
    png = (WEB / "icon-192.png").read_bytes()
    digest = source_hash([html, manifest, svg, png])
    gz = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)
    assets = (
        "// GENERADO por tools/build_web.py a partir de firmware/web/. No editar.\n"
        f"// fuente: {digest}\n"
        "#pragma once\n\n#include <pgmspace.h>\n#include <stdint.h>\n\n"
        f"// index.html: {len(html)} bytes -> {len(gz)} bytes comprimido\n"
        + c_bytes("kIndexHtmlGz", gz) + "\n"
        + c_text("kManifestJson", manifest) + "\n"
        + c_text("kIconSvg", svg) + "\n"
        + c_bytes("kIconPng", png)
    )

    files = sorted(LAYOUTS.glob("*.json"))
    entries = []
    texts = []
    for f in files:
        text = f.read_text(encoding="utf-8").strip()
        texts.append(f.name + text)
        entries.append(f'    {{"{f.stem}", R"RAW({text})RAW"}},')
    ldigest = source_hash(texts)
    layouts = (
        "// GENERADO por tools/build_web.py a partir de firmware/layouts/. No editar.\n"
        f"// fuente: {ldigest}\n"
        "#pragma once\n\n"
        "struct DefaultLayout {\n  const char* name;\n  const char* json;\n};\n\n"
        "static const DefaultLayout kDefaultLayouts[] = {\n" + "\n".join(entries) + "\n};\n\n"
        "static const int kDefaultLayoutCount = sizeof(kDefaultLayouts) / sizeof(kDefaultLayouts[0]);\n"
    )
    return {OUT_ASSETS: (assets, digest), OUT_LAYOUTS: (layouts, ldigest)}


def main():
    outputs = build()
    if "--check" in sys.argv:
        stale = []
        for path, (_, digest) in outputs.items():
            current = path.read_text(encoding="utf-8") if path.exists() else ""
            if f"// fuente: {digest}" not in current:
                stale.append(path.name)
        if stale:
            print("Desactualizado: " + ", ".join(stale) + ". Ejecuta python3 firmware/tools/build_web.py")
            sys.exit(1)
        print("Recursos web al día")
        return
    for path, (text, _) in outputs.items():
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            path.write_text(text, encoding="utf-8")
            print(f"escrito {path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
