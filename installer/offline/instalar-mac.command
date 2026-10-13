#!/bin/sh
# Instalador sin conexión de Märklin ESP32 (Linux y macOS).
# Conecta el ESP32 por USB y ejecuta este fichero.
cd "$(dirname "$0")" || exit 1
ESPTOOL=./esptool/esptool
chmod +x "$ESPTOOL" 2>/dev/null
# macOS: quitar la cuarentena de la descarga para poder ejecutar esptool.
command -v xattr >/dev/null 2>&1 && xattr -dr com.apple.quarantine ./esptool 2>/dev/null

echo "=== Instalador de Märklin ESP32 ==="
echo "Conecta el ESP32 por USB (cable de datos)."
echo "Buscando el ESP32..."
INFO=$("$ESPTOOL" chip_id 2>&1)
case "$INFO" in
  *"Chip is ESP32-S3"*) BIN=marklin-esp32s3.bin; CHIP="ESP32-S3" ;;
  *"Chip is ESP32-C3"*) BIN=marklin-esp32c3.bin; CHIP="ESP32-C3" ;;
  *"Chip is ESP32"*)    BIN=marklin-esp32.bin;   CHIP="ESP32" ;;
  *)
    echo "$INFO" | tail -5
    echo
    echo "No se encontró ningún ESP32. Comprueba el cable USB y, si hace falta,"
    echo "mantén pulsado el botón BOOT de la placa mientras vuelves a ejecutar esto."
    echo "En Linux puede hacer falta permiso para el puerto: sudo usermod -a -G dialout \$USER"
    exit 1 ;;
esac
echo "Encontrado: $CHIP"
printf "¿Borrar todo antes de instalar? (recomendado la primera vez; borra configuración y scripts) [s/N] "
read -r R
ERASE=""
case "$R" in s|S|si|SI|sí) ERASE="--erase-all" ;; esac
"$ESPTOOL" --chip auto --baud 460800 write_flash $ERASE 0x0 "firmware/$BIN" || {
  echo "La instalación falló. Prueba otra vez manteniendo pulsado BOOT."; exit 1; }
echo
echo "¡Listo! Conéctate con el móvil a la WiFi 'Marklin-ESP32' (contraseña marklin123)"
echo "y abre http://192.168.4.1 si la aplicación no se abre sola."
