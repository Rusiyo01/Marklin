# API REST y consola serie

Todo lo que hace la interfaz web se puede hacer con peticiones HTTP, por
ejemplo desde un programa en el PC, Home Assistant o `curl`. Las respuestas
son JSON; los errores devuelven `{"ok": false, "error": "..."}` con código
400.

Los parámetros van en la URL (`?loco=mm2:24&speed=500`); el cuerpo (texto)
solo se usa para scripts, órdenes y configuración.

## Estado

| Método y ruta | Descripción |
|---|---|
| `GET /api/state` | Estado completo: corriente (`power`: `on`/`off`/`unknown`), sobrecarga, locos, sensores, accesorios, scripts en marcha, equipos del bus, estadísticas CAN, WiFi y apps de Central Station conectadas (`lan`: `tcp`, `udp`, `frames`, `files`). |
| `GET /api/log?since=N` | Entradas del registro con número de secuencia mayor que N (monitor CAN, mensajes de scripts, errores). |

## Mando

| Método y ruta | Parámetros | Descripción |
|---|---|---|
| `POST /api/power` | `on=1` / `on=0` | GO / STOP. |
| `POST /api/emergency` | | Parada de emergencia: corta la vía, detiene los scripts y pone las locos a 0. |
| `POST /api/halt` | | Pone a 0 todas las locos (sin cortar la vía). |
| `POST /api/loco` | `loco=mm2:24` y uno o varios de: `speed=0..1000`, `dir=fwd\|rev\|toggle`, `fn=0..31&on=1\|0`, `estop=1`, `add=1` | Manda una loco. `add=1` la añade a la lista y consulta su estado. |
| `POST /api/accessory` | `addr=5` (o `mm:5`, `dcc:12`), `pos=0..3` | Desvío/señal: 1 recto/verde, 0 desviado/rojo, 2 amarillo, 3 blanco. |
| `POST /api/sensor` | `id=3&state=1` | Simula un sensor (para probar scripts sin trenes). |
| `POST /api/ping` | | Busca equipos en el bus CAN. |
| `POST /api/exec` | cuerpo: una orden | Orden inmediata del lenguaje de scripts, p. ej. `speed dcc:3 40%`. |

## Scripts

| Método y ruta | Descripción |
|---|---|
| `GET /api/scripts` | Lista `[{"name":..., "running":...}]`. |
| `GET /api/script?name=X` | Texto del script. |
| `POST /api/script?name=X` | Guarda el cuerpo como script. Responde además `valid` y, si hay error, `line` y `error`. |
| `POST /api/script/delete?name=X` | Borra el script (debe estar detenido). |
| `POST /api/check` | Comprueba el script del cuerpo sin guardarlo. |
| `POST /api/run?name=X` | Arranca el script. |
| `POST /api/scriptstop?name=X` / `?slot=N` / `?all=1` | Detiene scripts (y para sus locos). |

## Maquetas (C-Track Planner)

| Método y ruta | Descripción |
|---|---|
| `GET /api/layouts` | Lista `[{"name", "title", "active"}]`. |
| `GET /api/layout?name=X` | JSON de la maqueta (formato C-Track Planner + sección `marklin`). |
| `POST /api/layout?name=X` | Guarda la maqueta del cuerpo (máx. 48 KB). |
| `POST /api/layout/delete?name=X` | Borra la maqueta. |
| `POST /api/layout/activate?name=X` | La activa (sus nombres valen en los scripts); `name=` vacío = ninguna. |

## Configuración

| Método y ruta | Descripción |
|---|---|
| `POST /api/setup` | Asistente: cuerpo `{"locos":[{"name","loco"}], "layout":"...", "wifi":{"mode","ssid","password"}}`. Responde `restart: true` si cambia la WiFi. |
| `POST /api/locoreg?loco=mm2:24&name=cocodrilo&label=Cocodrilo%20SBB` | Registra o edita una loco (`old=` con la dirección anterior si cambia; `delete=1` para quitarla). |
| `GET /api/config` | Configuración actual (JSON). |
| `POST /api/config` | Valida, guarda y reinicia. Cuerpo: JSON (ver [configuracion.md](configuracion.md)). |
| `POST /api/restart` | Reinicia el ESP32. |

## Red de Central Station (CS2)

Además de la API REST, el ESP32 imita el interfaz de red de una Central
Station (ver [railcontrol-pro.md](railcontrol-pro.md)):

| Puerto / ruta | Uso |
|---|---|
| TCP 15731 | Tramas CAN de 13 bytes en los dos sentidos (hasta 4 apps). |
| UDP 15731 → 15730 | Lo mismo por UDP; la respuesta va al puerto 15730 de la app. |
| `GET /config/lokomotive.cs2` | Locos registradas en formato CS2. |
| `GET /config/magnetartikel.cs2` | Desvíos de la maqueta activa. |
| `GET /config/gleisbild.cs2`, `/config/gleisbilder/<página>.cs2` | Plano de vías (panel de desvíos y sensores). |

## Ejemplos con curl

```sh
ESP=http://192.168.4.1
curl -X POST "$ESP/api/power?on=1"
curl -X POST "$ESP/api/loco?loco=mm2:24&dir=fwd"
curl -X POST "$ESP/api/loco?loco=mm2:24&speed=400"
curl -X POST "$ESP/api/loco?loco=mm2:24&fn=0&on=1"
curl -X POST "$ESP/api/accessory?addr=5&pos=0"
curl -X POST --data-binary @mi_script.txt "$ESP/api/script?name=mi_script"
curl -X POST "$ESP/api/run?name=mi_script"
curl -s "$ESP/api/state" | python3 -m json.tool
```

## Consola serie

Conecta el ESP32 por USB y abre un monitor serie a **115200 baudios**
(`pio device monitor`). Al arrancar muestra la IP y los eventos. Órdenes:

| Orden | Descripción |
|---|---|
| `help` | Ayuda. |
| `status` | Corriente, estadísticas CAN, equipos, locos, sensores y scripts. |
| `scripts` | Lista de scripts guardados. |
| `show <script>` | Muestra un script. |
| `run <script>` | Arranca un script. |
| `stop <script>` / `stop all` | Detiene scripts. |
| `!` | Parada de emergencia. |
| `ping` | Busca equipos en el bus. |
| `monitor on` / `off` | Muestra las tramas CAN en la consola. |
| `config` | Muestra la configuración. |
| `restart` | Reinicia. |
| cualquier orden inmediata | `power on`, `speed mm2:24 50%`, `dir dcc:3 rev`, `func mfx:5 0 on`, `switch 5 straight`, `halt`... |
