#!/bin/bash
# Empaqueta el instalador a partir de los binarios combinados (merge_bin):
#   site/   página de instalación web (ESP Web Tools) para GitHub Pages
#   dist/   marklin-instalador-{windows,mac,linux}.zip (sin conexión, con esptool)
#
# Uso: package_installer.sh <carpeta con marklin-esp32*.bin> <carpeta de salida>
set -euo pipefail
BINS=$(cd "$1" && pwd)
OUT=$(mkdir -p "$2" && cd "$2" && pwd)
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
VERSION=$(sed -n 's/#define FW_VERSION "\(.*\)"/\1/p' "$ROOT/firmware/src/version.h")
ESPTOOL_VER=v4.8.1
CACHE=${ESPTOOL_CACHE:-$OUT/.esptool}

for chip in esp32 esp32s3 esp32c3; do
  test -s "$BINS/marklin-$chip.bin" || { echo "falta $BINS/marklin-$chip.bin" >&2; exit 1; }
done

# ---- Página de instalación web ----
rm -rf "$OUT/site" && mkdir -p "$OUT/site"
sed "s/__VERSION__/$VERSION/g" "$ROOT/installer/index.html" > "$OUT/site/index.html"
sed "s/__VERSION__/$VERSION/g" "$ROOT/installer/manifest.json" > "$OUT/site/manifest.json"
cp "$ROOT/installer/icon.svg" "$BINS"/marklin-esp32*.bin "$OUT/site/"

# ---- Paquetes sin conexión ----
mkdir -p "$CACHE" "$OUT/dist"
fetch() {  # fetch <sufijo de esptool>
  local f="$CACHE/esptool-$1.zip"
  [ -s "$f" ] || curl -fsSL -o "$f" \
    "https://github.com/espressif/esptool/releases/download/$ESPTOOL_VER/esptool-$ESPTOOL_VER-$1.zip"
  echo "$f"
}
pack() {  # pack <so> <sufijo esptool> <binario esptool> <scripts...>
  local os=$1 suf=$2 exe=$3; shift 3
  local st="$OUT/stage-$os/marklin-instalador"
  rm -rf "$OUT/stage-$os" && mkdir -p "$st/firmware" "$st/esptool"
  cp "$BINS"/marklin-esp32*.bin "$st/firmware/"
  sed "s/__VERSION__/$VERSION/g" "$ROOT/installer/offline/LEEME.txt" > "$st/LEEME.txt"
  for s in "$@"; do cp "$ROOT/installer/offline/$s" "$st/"; done
  local zip; zip=$(fetch "$suf")
  unzip -q -j -o "$zip" "esptool-$suf/$exe" -d "$st/esptool"
  unzip -q -j -o "$zip" "esptool-$suf/LICENSE" -d "$st/esptool"
  chmod +x "$st/esptool/$exe" "$st"/*.sh "$st"/*.command 2>/dev/null || true
  if [ "$os" = windows ]; then  # fin de línea CRLF para los .bat y .txt
    for f in "$st"/*.bat "$st"/LEEME.txt; do sed -i 's/\r\?$/\r/' "$f"; done
  fi
  rm -f "$OUT/dist/marklin-instalador-$os.zip"
  (cd "$OUT/stage-$os" && zip -q -r -X "$OUT/dist/marklin-instalador-$os.zip" marklin-instalador)
  rm -rf "$OUT/stage-$os"
  echo "dist/marklin-instalador-$os.zip"
}
pack windows win64 esptool.exe instalar-windows.bat
pack mac macos esptool instalar-mac.command
pack linux linux-amd64 esptool instalar-linux.sh
cp "$BINS"/marklin-esp32*.bin "$OUT/dist/"
echo "versión $VERSION empaquetada en $OUT"
