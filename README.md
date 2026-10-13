# Märklin ESP32 — control automático de trenes con la Gleisbox de la MS2

Firmware para un **ESP32** que se conecta al **bus CAN de la Gleisbox**
(60113/60116, el módulo que viene con la **Märklin Mobile Station 2**) y
permite mover las locomotoras **de forma automática** con scripts sencillos,
además de manejarlas desde el móvil o el PC.

```
 MS2 ──cable── Gleisbox ──── vía
                  │
              bus CAN (CAN-H, CAN-L, GND)
                  │
        SN65HVD230 ── ESP32  ── WiFi ──► móvil / PC
                        └── sensores (reed, IR, vías de contacto)
```

## Instalación en 3 pasos

1. **Instala** el firmware desde el navegador (Chrome/Edge) en
   **https://rusiyo01.github.io/Marklin/** — o con el instalador sin conexión
   para Windows, Mac o Linux. Todo viene preconfigurado.
2. **Conéctate** con el móvil a la WiFi **`Marklin-ESP32`** (contraseña
   `marklin123`): la aplicación se abre sola.
3. **Sigue el asistente**: comprueba la Gleisbox, recoge tus locos, elige o
   sube tu maqueta de **C-Track Planner** y tu WiFi.

Detalles en [docs/instalacion.md](docs/instalacion.md). Antes de conectar
el ESP32 a la Gleisbox, lee la [guía de montaje](docs/hardware.md).

## Qué hace

- **Habla el protocolo CAN de Märklin** (el mismo de la MS2/CS2/CS3): corriente
  de vía, velocidad, sentido, funciones F0–F31, desvíos y señales, para locos
  **MM2, DCC y mfx**.
- **Funciona con la MS2 conectada** (y escucha lo que hace: las locos que
  mueves con la MS2 aparecen solas) **o sin ella** (despierta la Gleisbox
  por sí mismo).
- **Automatización con scripts**: esperas, sensores con *timeout*, rampas de
  aceleración, bucles, saltos condicionales y al azar, y **hasta 4 trenes a
  la vez** que se coordinan reservando tramos de vía (`lock`/`unlock`).
- **Aplicación web** (sin Internet, desde el punto de acceso del ESP32, que se
  abre sola al conectarse y se puede añadir a la pantalla de inicio): mandos
  de locos, editor de scripts con comprobación de errores, sensores (con
  simulación para probar sin trenes), desvíos, monitor CAN, ajustes y un
  asistente de primera configuración.
- **Maquetas de C-Track Planner**: sube tu plano (.json o la app .html) y
  úsalo como cuadro de mando: desvíos con un toque, sensores en directo y
  nombres de desvíos/sensores en los scripts. Trae 3 maquetas de ejemplo ya
  configuradas.
- **Sensores**: en GPIO del ESP32 (reed, Hall, infrarrojos, vías de contacto
  con optoacoplador) o por CAN (Link S88).
- **Seguridad**: parada de emergencia (botón BOOT, web, consola), los scripts
  detenidos o con error paran sus locos, *timeouts* en las esperas.
- **API REST** y **consola serie** para integrarlo con otros programas.

```
# Lanzadera entre dos estaciones
loco tren mm2 24
sensor estacion_a 1
sensor estacion_b 2

power on
label inicio
  dir tren fwd
  ramp tren 50% 3s
  wait sensor estacion_b on timeout 2m
  ramp tren 0 1500ms
  wait random 10s 30s
  dir tren rev
  ramp tren 50% 3s
  wait sensor estacion_a on timeout 2m
  ramp tren 0 1500ms
  wait random 10s 30s
goto inicio
```

## Material

| Componente | Precio aprox. |
|---|---|
| Placa ESP32 (DevKitC / WROOM-32; también ESP32-S3 o ESP32-C3) | 5–10 € |
| Módulo transceptor CAN **SN65HVD230** (3,3 V) | 2–5 € |
| Cable de 3 hilos hasta el bus CAN de la Gleisbox | — |
| Opcional: sensores reed/Hall/IR, optoacopladores PC817 | — |

## Puesta en marcha

1. **Montaje** — sigue [docs/hardware.md](docs/hardware.md). Resumen:

   | ESP32 | SN65HVD230 | Gleisbox |
   |---|---|---|
   | 3V3 | VCC | — |
   | GND | GND | **GND** |
   | GPIO 5 | CTX | — |
   | GPIO 4 | CRX | — |
   | — | CANH | CAN-H |
   | — | CANL | CAN-L |

   > ⚠️ El conector de la Gleisbox lleva también ~18 V para la MS2 y los
   > patillajes publicados se contradicen: **identifica las patillas con un
   > multímetro** antes de conectar (explicado en la guía).

2. **Grabar el firmware**: lo más fácil es la [página de instalación](https://rusiyo01.github.io/Marklin/)
   (ver [docs/instalacion.md](docs/instalacion.md)). Para compilarlo tú, con
   [PlatformIO](https://platformio.org/) (extensión de VS Code o línea de órdenes):

   ```sh
   cd firmware
   pio run -t upload            # ESP32 normal
   pio run -e esp32-s3 -t upload   # o esp32-c3
   pio device monitor           # consola serie, 115200 baudios
   ```

   También puedes descargar los binarios compilados por la CI (pestaña
   *Actions* de GitHub → *Artifacts*).

   <details><summary>¿Prefieres el IDE de Arduino?</summary>

   1. Instala el soporte de placas **esp32** de Espressif y la biblioteca
      **ArduinoJson** (versión 7) desde el gestor de bibliotecas.
   2. Copia las carpetas `firmware/lib/marklin` y `firmware/lib/automation`
      a tu carpeta `Arduino/libraries`.
   3. Crea una carpeta de sketch `marklin_esp32` con todos los ficheros de
      `firmware/src` y un fichero vacío `marklin_esp32.ino`.
   4. En *Herramientas → Partition Scheme* elige **Minimal SPIFFS** y graba.
   </details>

3. **Conectarse**: busca la red WiFi **`Marklin-ESP32`** (contraseña
   `marklin123`); la aplicación se abre sola (si no, **http://192.168.4.1/**)
   y arranca el asistente de configuración.

4. **Comprobar el bus**: en *Ajustes* → *Buscar equipos* deben aparecer la
   Gleisbox (y la MS2 si está conectada). Mueve una loco con la MS2: debe
   aparecer en *Mando*.

5. **Primera prueba**: en *Mando* pulsa **GO**, añade tu loco (p. ej.
   `mm2:24`) y muévela. Después prueba el script `demo_tiempo` en
   *Automático* (cambia antes la dirección de la loco).

## Documentación

| Documento | Contenido |
|---|---|
| [docs/instalacion.md](docs/instalacion.md) | Instalación desde el navegador o sin conexión, asistente, actualizar y publicar versiones. |
| [docs/maquetas.md](docs/maquetas.md) | Subir maquetas de C-Track Planner, asignar desvíos y sensores, ejemplos incluidos. |
| [docs/investigacion.md](docs/investigacion.md) | Cómo funciona MS2 + Gleisbox, el protocolo CAN de Märklin, direcciones, arranque de la Gleisbox y fuentes. |
| [docs/hardware.md](docs/hardware.md) | Material, cómo llegar al bus CAN, identificar patillas, conexionado, terminación, alimentación, sensores y emergencia. |
| [docs/scripts.md](docs/scripts.md) | Referencia completa del lenguaje de scripts y ejemplos. |
| [docs/configuracion.md](docs/configuracion.md) | WiFi, pines, locos y sensores con nombre, autoarranque. |
| [docs/api.md](docs/api.md) | API REST y consola serie. |

## Estructura del proyecto

```
firmware/
  platformio.ini          placas: esp32dev, esp32-s3, esp32-c3
  lib/marklin/            protocolo CAN de Märklin + controlador (portable)
  lib/automation/         lenguaje de scripts y motor de ejecución (portable)
  src/                    firmware del ESP32: TWAI, web, API, consola, sensores, maquetas
  web/                    fuentes de la aplicación web (se empaqueta con tools/build_web.py)
  layouts/                maquetas de ejemplo (generadas con tools/make_layouts.js)
  tools/                  empaquetado de la web y del instalador
  test_host/              tests en el PC (make -C firmware/test_host; node test_layouts.js)
installer/                página de instalación web y scripts del instalador sin conexión
docs/                     documentación
.github/workflows/ci.yml  tests, compilación de las tres placas, instalador, Pages y releases
```

La lógica del protocolo y de los scripts no depende de Arduino y se prueba
en el PC (`make -C firmware/test_host`: 31 tests, incluidos los ejemplos y
las tramas exactas que usan can2udp/Railuino para la Gleisbox).

## Estado y limitaciones

- ✅ Compila para ESP32, ESP32-S3 y ESP32-C3 (PlatformIO, Arduino-ESP32 2.0.17);
  tests del protocolo, de los scripts y de las maquetas pasan; la aplicación
  web (asistente, maquetas) está probada en navegador contra una API
  simulada, y la importación con ficheros exportados por C-Track Planner.
- ⚠️ **Aún no probado con una Gleisbox real.** El protocolo está contrastado
  con proyectos que sí funcionan con Gleisbox (Railuino, can2udp), pero haz
  las primeras pruebas con cuidado: un tramo recto, velocidad baja y la mano
  cerca del botón STOP de la MS2.
- Locos **mfx**: se manejan con el SID que les asigna la MS2 (con la MS2
  conectada). El registro mfx sin MS2 no está implementado; sin MS2 usa MM2
  o DCC.
- La Gleisbox no tiene entradas S88: los sensores van al ESP32 o a un Link S88.

## Créditos y referencias

Basado en la documentación pública del protocolo CAN de Märklin y en el
trabajo de la comunidad: [Railuino](https://github.com/BOBILLEChristophe/Railuino)
(Jörg Pleumann, Christophe Bobille), [GBert/railroad](https://github.com/GBert/railroad)
(can2udp, can-monitor), Stefan Krauß ("Gleisbox als Zentrale") y los foros
Stummiforum y Märklin-Users. Märklin, Mobile Station y mfx son marcas de
Gebr. Märklin & Cie. GmbH; este proyecto no está afiliado a Märklin.
