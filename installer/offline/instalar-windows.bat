@echo off
rem Instalador sin conexión de Marklin ESP32 (Windows).
rem Conecta el ESP32 por USB y haz doble clic en este fichero.
chcp 65001 >nul
cd /d "%~dp0"
echo === Instalador de Marklin ESP32 ===
echo Conecta el ESP32 por USB (cable de datos).
echo Buscando el ESP32...
esptool\esptool.exe chip_id > "%TEMP%\marklin_chip.txt" 2>&1
set BIN=
findstr /C:"Chip is ESP32-S3" "%TEMP%\marklin_chip.txt" >nul && set BIN=marklin-esp32s3.bin
if not defined BIN findstr /C:"Chip is ESP32-C3" "%TEMP%\marklin_chip.txt" >nul && set BIN=marklin-esp32c3.bin
if not defined BIN findstr /C:"Chip is ESP32" "%TEMP%\marklin_chip.txt" >nul && set BIN=marklin-esp32.bin
if not defined BIN (
  type "%TEMP%\marklin_chip.txt"
  echo.
  echo No se encontro ningun ESP32. Comprueba el cable USB y el controlador
  echo ^(CP210x o CH340^) y, si hace falta, manten pulsado BOOT al volver a intentarlo.
  pause
  exit /b 1
)
echo Encontrado: %BIN%
set ERASE=
set /p R="Borrar todo antes de instalar? (recomendado la primera vez) [s/N] "
if /i "%R%"=="s" set ERASE=--erase-all
esptool\esptool.exe --chip auto --baud 460800 write_flash %ERASE% 0x0 firmware\%BIN%
if errorlevel 1 (
  echo La instalacion fallo. Prueba otra vez manteniendo pulsado BOOT.
  pause
  exit /b 1
)
echo.
echo Listo! Conectate con el movil a la WiFi "Marklin-ESP32" (contrasena marklin123)
echo y abre http://192.168.4.1 si la aplicacion no se abre sola.
pause
