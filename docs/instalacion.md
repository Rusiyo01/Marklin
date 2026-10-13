# Instalación

El firmware se instala **ya preconfigurado**: crea su propia red WiFi, trae
scripts y maquetas de ejemplo, y la primera vez abre un **asistente** que lo
deja listo en un minuto. No hace falta instalar ningún programa de
desarrollo.

## Opción 1 · Desde el navegador (recomendada)

1. Abre la **página de instalación** con **Chrome o Edge** en un ordenador
   (Windows, Mac o Linux): `https://rusiyo01.github.io/Marklin/`
2. Conecta el ESP32 por USB con un cable de datos.
3. Pulsa **Instalar Märklin ESP32**, elige el puerto del ESP32 y confirma.
   La primera vez acepta *borrar el dispositivo*.

La página detecta sola si es un ESP32, ESP32-S3 o ESP32-C3.

> La página se publica automáticamente desde GitHub. Si todavía no está
> activa, el dueño del repositorio debe hacerlo una vez: en GitHub,
> **Settings → Pages → Source: GitHub Actions**. Después, cada cambio en la
> rama principal la actualiza.

## Opción 2 · Instalador sin conexión

Para Firefox/Safari o sin Internet en el ordenador del taller:

1. Descarga el paquete de tu sistema (en la página de instalación o en la
   sección **Releases** de GitHub): `marklin-instalador-windows.zip`,
   `marklin-instalador-mac.zip` o `marklin-instalador-linux.zip`.
2. Descomprímelo, conecta el ESP32 y ejecuta:
   - **Windows**: doble clic en `instalar-windows.bat`.
   - **macOS**: doble clic en `instalar-mac.command` (si macOS lo bloquea:
     clic derecho → Abrir).
   - **Linux**: `sh instalar-linux.sh`.

El paquete incluye `esptool`, la herramienta oficial de Espressif, y los
tres firmwares; el instalador detecta el modelo de ESP32 y pregunta si
quieres borrarlo todo antes (recomendado la primera vez).

## Opción 3 · Compilar tú mismo

Ver el README (PlatformIO o IDE de Arduino).

## Si no aparece el puerto del ESP32

- Usa un cable USB **de datos** (muchos cables de carga no llevan datos).
- Instala el controlador USB de tu placa: **CP210x** (Silicon Labs) o
  **CH340** (WCH). Las placas ESP32-S3/C3 con USB nativo no lo necesitan.
- Mantén pulsado **BOOT** mientras empieza la instalación y suéltalo cuando
  empiece a escribir.

## Primer arranque

1. Desde el portátil o el móvil, conéctate a la WiFi **`Marklin-ESP32`**
   (contraseña `marklin123`) y abre **http://192.168.4.1** (en el móvil se
   abre sola). Si el móvil avisa de que "la red no tiene
   Internet", elige **mantener la conexión**.
2. El **asistente** te guía:
   1. **Conexión**: comprueba el bus CAN y busca la Gleisbox y la MS2.
   2. **Locomotoras**: mueve cada loco con la MS2 y aparece sola (o añádela
      con su dirección); dales un nombre corto.
   3. **Maqueta**: elige una de ejemplo o sube la tuya de C-Track Planner
      ([maquetas.md](maquetas.md)).
   4. **WiFi**: únelo a tu WiFi de casa (recomendado para el portátil: se
      abre con **http://marklin.local** y no pierdes Internet) o sigue con la
      red propia del ESP32.
   5. **Listo**: crea un script de prueba con tu primera loco.
3. Para usarla como una app: en el portátil, Chrome/Edge → **Crear acceso
   directo… → Abrir como ventana**; en el móvil, **Añadir a pantalla de
   inicio**. Su uso está en [puesto-de-mando.md](puesto-de-mando.md).

El asistente se puede volver a abrir en **Ajustes → Asistente de
configuración**.

## Qué viene preconfigurado

| | |
|---|---|
| WiFi | Red propia `Marklin-ESP32` / `marklin123`, portal cautivo, `http://marklin.local` |
| Bus CAN | 250 kbit/s, TX = GPIO 5, RX = GPIO 4; despierta la Gleisbox si no hay MS2 |
| Emergencia | Botón BOOT de la placa |
| Scripts | `demo_tiempo`, `lanzadera`, `puente_tren1/2`, `estacion_aleatoria` |
| Maquetas | `lanzadera`, `estacion` y `ramal_elevado`, con desvíos y sensores ya asignados que coinciden con los scripts de ejemplo |

## Actualizar

Vuelve a instalar desde la página web (o el paquete sin conexión) **sin**
borrar el dispositivo: se conservan la configuración, los scripts y las
maquetas.

## Publicar una versión nueva (para el dueño del repositorio)

1. Cambia `FW_VERSION` en `firmware/src/version.h`.
2. Crea y sube una etiqueta: `git tag v1.2.0 && git push origin v1.2.0`.
3. La CI compila las tres placas, actualiza la página de instalación y crea
   una *Release* con los paquetes sin conexión.
