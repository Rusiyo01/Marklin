# Configuración

La configuración se guarda en el ESP32 (fichero `/config.json`) y se edita
desde la pestaña **Ajustes** de la interfaz web. Al pulsar *Guardar y
reiniciar* se valida (si hay un error se indica cuál y no se guarda) y el
ESP32 se reinicia para aplicarla.

## Ejemplo completo

```json
{
  "wifi": {
    "mode": "sta",
    "ssid": "MiWifi",
    "password": "secreto",
    "apSsid": "Marklin-ESP32",
    "apPassword": "marklin123",
    "hostname": "marklin"
  },
  "can": {
    "txPin": 5,
    "rxPin": 4,
    "gleisboxInit": "auto",
    "protocols": ["mm2", "mfx", "dcc"],
    "accessoryPulseMs": 250
  },
  "estopPin": 0,
  "locos": [
    { "name": "cocodrilo", "loco": "mm2:24" },
    { "name": "ice", "loco": "dcc:3" },
    { "name": "br89", "loco": "mfx:5" }
  ],
  "sensors": [
    { "id": 1, "name": "estacion_a", "pin": 32, "activeLow": true, "debounceMs": 30 },
    { "id": 2, "name": "estacion_b", "pin": 33, "activeLow": true },
    { "id": 3, "name": "antes_puente", "pin": 25 },
    { "id": 1005, "name": "s88_modulo1_c5" }
  ],
  "autostart": [],
  "layout": "estacion",
  "setupDone": true
}
```

## Campos

### `wifi`

| Campo | Por defecto | Descripción |
|---|---|---|
| `mode` | `"ap"` | `"ap"`: el ESP32 crea su propia red WiFi. `"sta"`: se une a tu red; si no lo consigue en 15 s, vuelve a crear su red para que no pierdas el acceso. |
| `ssid`, `password` | | Tu red WiFi (modo `"sta"`). |
| `apSsid`, `apPassword` | `"Marklin-ESP32"`, `"marklin123"` | Red propia del ESP32. La contraseña debe tener al menos 8 caracteres (o vacía = red abierta). |
| `hostname` | `"marklin"` | Nombre en la red: `http://marklin.local/` (mDNS). |

En modo punto de acceso la interfaz está en **http://192.168.4.1/**. En
modo `"sta"`, la IP aparece en la consola serie al arrancar (o usa
`http://marklin.local/`).

### `can`

| Campo | Por defecto | Descripción |
|---|---|---|
| `txPin`, `rxPin` | `5`, `4` | GPIO conectados a CTX/CRX del transceptor. |
| `gleisboxInit` | `"auto"` | `"auto"`: si al arrancar no se detecta una MS2, envía la secuencia que despierta la Gleisbox. `"always"`: la envía siempre. `"never"`: nunca (usa esto si trabajas siempre con la MS2 y prefieres no enviarla). |
| `protocols` | `["mm2","mfx","dcc"]` | Protocolos de vía que se habilitan al despertar la Gleisbox. |
| `accessoryPulseMs` | `250` | Tiempo que se mantiene activada la bobina de un desvío (20–5000 ms). |

### `estopPin`

GPIO del pulsador de parada de emergencia (a GND, con pull-up interna). Por
defecto el botón BOOT de la placa (GPIO 0; GPIO 9 en ESP32-C3). `-1` lo
desactiva.

### `locos`

Lista de locos con nombre. Aparecen siempre en la pestaña Mando y su nombre
se puede usar directamente en los scripts sin definirlo con `loco`.

| Campo | Descripción |
|---|---|
| `name` | Nombre (minúsculas, números, `_` y `-`). |
| `loco` | Dirección: `mm2:24`, `dcc:3`, `mfx:5` (para mfx, el SID que le asignó la MS2; mueve la loco con la MS2 y mira la pestaña Mando para verlo). |

### `sensors`

| Campo | Por defecto | Descripción |
|---|---|---|
| `id` | (obligatorio) | Número del sensor en los scripts (> 0). |
| `name` | | Nombre opcional, usable en los scripts. |
| `pin` | `-1` | GPIO del sensor. `-1` = no está en el ESP32 (llega por CAN desde un Link S88, o solo para simular). |
| `activeLow` | `true` | `true`: activo cuando el pin está a GND (contactos reed, optoacopladores; usa pull-up interna). `false`: activo a nivel alto (usa pull-down interna). |
| `debounceMs` | `30` | Tiempo de filtrado de rebotes. |

### `autostart`

Lista de scripts que se arrancan solos 2,5 s después de encender el ESP32,
por ejemplo `["puente_tren1", "puente_tren2"]` para una maqueta de
exposición.

> ⚠️ Con `autostart`, los trenes pueden empezar a moverse en cuanto se
> enciende el sistema. Úsalo solo cuando la maqueta esté preparada.

### `layout` y `setupDone`

- `layout`: maqueta activa (ver [maquetas.md](maquetas.md)); sus nombres de
  desvíos y sensores se pueden usar en los scripts. `""` = ninguna.
- `setupDone`: `false` hace que el asistente de primera configuración se
  abra al entrar en la web.

## Volver a la configuración de fábrica

Guarda `{}` como configuración: todos los campos toman su valor por
defecto (WiFi propia `Marklin-ESP32` / `marklin123`) y vuelve a aparecer el
asistente. Si no puedes acceder
a la web, borra la memoria con `pio run -t erase` y vuelve a grabar el
firmware.
