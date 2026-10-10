# Investigación: controlar la Gleisbox de la MS2 desde un ESP32

Este documento resume lo que hace falta saber para conectar un
microcontrolador al sistema **Märklin Mobile Station 2 (MS2) + Gleisbox
60113/60116** y mover trenes de forma automática. Las fuentes están al final.

## 1. Cómo funciona el sistema MS2 + Gleisbox

```
  ┌───────────────┐   cable de 10 polos     ┌────────────────┐   vía (señal digital)
  │ Mobile        │◄───────────────────────►│ Gleisbox       │──────────────────────► locos,
  │ Station 2     │  CAN-H, CAN-L, GND,     │ 60113 / 60116  │   MM2 / mfx / DCC        desvíos
  │ (mando)       │  alimentación ~18 V     │ (central+booster)│
  └───────────────┘                         └────────────────┘
                                                 ▲   ▲
                                 fuente 18-19 V ─┘   │
                                                     │ mismo bus CAN
                                         ┌───────────┴──────────┐
                                         │ ESP32 + transceptor  │   ← este proyecto
                                         │ CAN (SN65HVD230)     │
                                         └──────────────────────┘
```

- La **MS2 no genera la señal de vía**: es solo el mando. Quien genera la
  señal digital (MM2, mfx y DCC) y da potencia a la vía es la **Gleisbox**
  (60113 en los primeros sets, 60116 en los más recientes).
- MS2 y Gleisbox hablan por un **bus CAN** con el mismo protocolo que la
  Central Station 2/3 ("protocolo CS2"). El cable de la MS2 lleva el bus CAN
  y la alimentación de la MS2.
- Por tanto, **cualquier equipo conectado a ese bus CAN puede dar las mismas
  órdenes que la MS2**: corriente de vía, velocidad, sentido, funciones y
  desvíos. Es lo que hacen proyectos como Railuino (Arduino), can2udp
  (Raspberry Pi + Rocrail) o la "Gleisbox como central" de Stefan Krauß.

### Opciones que se descartaron

| Opción | Por qué no |
|---|---|
| Simular las pulsaciones de la MS2 | Frágil, no da información de estado. |
| Sustituir la Gleisbox por un booster DCC propio (DCC-EX, etc.) | Pierdes mfx/MM2 y el hardware Märklin que ya tienes. |
| Comprar una CS2/CS3 | Cara; el objetivo es aprovechar la MS2. |
| **Hablar CAN con la Gleisbox (elegida)** | Barato (ESP32 + transceptor de ~2 €), oficial a nivel de protocolo, compatible con la MS2 conectada a la vez. |

El ESP32 es ideal porque lleva un **controlador CAN integrado (TWAI)**: solo
necesita un transceptor externo (SN65HVD230, a 3,3 V). No hace falta un
MCP2515.

## 2. Capa física

| Parámetro | Valor |
|---|---|
| Estándar | CAN 2.0B, identificador **extendido de 29 bits** |
| Velocidad | **250 kbit/s** |
| Conector en la Gleisbox | Toma de **10 polos** para la MS2 (tipo mini-DIN) |
| Señales útiles | **CAN-H, CAN-L y GND** (la toma lleva además la alimentación de la MS2, ~18 V) |
| Terminación | Bus terminado (120 Ω en los extremos) |

**Patillaje del conector.** Märklin no publica el patillaje y en los foros
hay datos **contradictorios**: un usuario indica *pin 4 = CAN-H, pin 8 =
CAN-L, pin 2 = GND, pin 1 = alimentación*, y otro indica pin 1 = GND y
pin 2 = V+. Por eso la guía de montaje ([hardware.md](hardware.md)) explica
cómo **identificar las patillas con un multímetro** antes de conectar nada.
Stefan Krauß, que convirtió su Gleisbox en central para el PC, confirma que
solo hacen falta CAN-H, CAN-L y masa, y que da igual cuál de las tomas para
MS2 de la Gleisbox se use. El conector de 10 polos de la MS2 no se vende
suelto.

## 3. El protocolo CAN de Märklin

Referencia oficial: documento *"Kommunikation mit der Central Station 2 –
CAN-Protokoll"* (cs2CAN-Protokoll-2_0.pdf) de Märklin. Los detalles de
abajo se han verificado contra el código de **can2udp/can-monitor** (GBert)
y **Railuino**.

### 3.1 Identificador (29 bits)

```
 bits 28..25   24..17     16         15..0
┌──────────┬──────────┬──────────┬──────────────┐
│ prioridad│ comando  │ respuesta│    hash      │
│  (4 bit) │ (8 bit)  │  (1 bit) │   (16 bit)   │
└──────────┴──────────┴──────────┴──────────────┘
ID = (prio << 25) | (comando << 17) | (resp << 16) | hash
```

- **Respuesta**: 0 en las órdenes; 1 en las respuestas/confirmaciones (la
  Gleisbox confirma cada orden con el mismo contenido y el bit a 1).
- **Hash**: identifica al emisor y evita colisiones. Se calcula a partir del
  UID de 32 bits del equipo, y los bits 7..9 deben valer `0b110` para
  distinguirse de las tramas de la antigua CS1:

  ```c
  hash = (uid >> 16) ^ (uid & 0xFFFF);
  hash = ((hash << 3) & 0xFF00) | 0x0300 | (hash & 0x7F);
  ```
  (Fórmula idéntica en can2udp; Railuino usa un número aleatorio con el
  mismo patrón: `(rand & 0xFF7F) | 0x0300`.) El firmware deriva su UID de
  la MAC del ESP32.
- Datos: 0 a 8 bytes, enteros en **big-endian**. Casi todas las órdenes
  empiezan con un **UID de 32 bits** (dirección de loco/accesorio, o 0 =
  todos).

### 3.2 Órdenes usadas por el firmware

| Comando | Nombre | DLC | Datos |
|---|---|---|---|
| `0x00` | Sistema | 5 | UID(4) + subcomando: `0x00` STOP, `0x01` GO, `0x02` HALT, `0x03` parada de emergencia de loco |
| `0x00` | Sistema: habilitar protocolos | 6 | UID 0 + `0x08` + máscara (bit0 MM2, bit1 mfx, bit2 DCC) |
| `0x00` | Sistema: consulta STOP/GO | 4 | UID 0 |
| `0x00` | Sistema: sobrecarga (la envía la Gleisbox) | 6 | UID + `0x0A` + canal |
| `0x04` | Velocidad de loco | 6 | UID(4) + velocidad(2) **0..1000** (DLC 4 = consulta) |
| `0x05` | Sentido de loco | 5 | UID(4) + `0` mantener, `1` adelante, `2` atrás, `3` cambiar |
| `0x06` | Función de loco | 6 | UID(4) + nº función + valor (0/1) |
| `0x0B` | Accesorio (desvío/señal) | 6 | UID(4) + posición (`0` rojo/desviado, `1` verde/recto, `2`, `3`) + corriente (1 = activar, 0 = desactivar) |
| `0x11` | Evento S88 (retromódulo) | 8 | módulo(2) + contacto(2) + estado anterior + estado nuevo + tiempo(2) |
| `0x18` | Ping | 0 / 8 | Respuesta: UID(4) + versión(2) + tipo de equipo(2) |
| `0x1B` | Bootloader | 5 | UID 0 + `0x11` → "arranque" de la Gleisbox |

Detalles importantes:

- **Velocidad**: escala 0..1000 sea cual sea el protocolo de la loco (la
  Gleisbox la convierte a pasos MM2/DCC/mfx).
- **Sentido**: al cambiar de sentido la loco se detiene. El firmware pone la
  velocidad a 0 antes de cambiarlo para que el estado sea siempre coherente.
- **Accesorios**: se activa la bobina con corriente = 1 y, pasado el tiempo
  de conmutación, se envía la misma orden con corriente = 0 (como hace
  Railuino). El firmware lo hace automáticamente (por defecto 250 ms).

### 3.3 Direcciones (UID) de locos y accesorios

| Tipo | Rango de UID | Ejemplo |
|---|---|---|
| Loco MM2 | `0x0001`–`0x03FF` (dirección = UID) | MM2 24 → `0x0018` |
| Loco SX1 | `0x0800`–`0x0BFF` | |
| Accesorio MM | `0x3000`–`0x33FF` (dirección 1 = `0x3000`) | desvío 5 → `0x3004` |
| Accesorio DCC | `0x3800`–`0x3FFF` (dirección 1 = `0x3800`) | |
| Loco mfx | `0x4000` + SID | SID 5 → `0x4005` |
| Loco DCC | `0xC000` + dirección | DCC 3 → `0xC003` |

En los scripts se escriben como `mm2:24`, `dcc:3`, `mfx:5`, y los
accesorios como `5` (MM), `mm:5` o `dcc:12`.

### 3.4 Arranque de la Gleisbox sin MS2

Al encenderse, la Gleisbox espera en su *bootloader* hasta que un mando le
dice que arranque. La MS2 lo hace automáticamente; si el ESP32 trabaja
**sin MS2**, debe enviar él mismo estas dos tramas (son exactamente las que
envía can2udp y Railuino):

```
ID 0x0036xxxx  DLC 5  00 00 00 00 11      ← bootloader: arrancar aplicación
ID 0x0000xxxx  DLC 6  00 00 00 00 08 07   ← habilitar MM2 + mfx + DCC
```

El firmware, en modo `auto`, hace un *ping* al arrancar y, si no responde
ninguna MS2, envía esta secuencia. Se puede forzar (`always`) o desactivar
(`never`) en la configuración.

### 3.5 Tipos de equipo (respuesta al ping)

| Tipo | Equipo |
|---|---|
| `0x0010`, `0x0011` | Gleisbox |
| `0x0020` | Connect 6021 |
| `0x0030`–`0x0034` | MS2 |
| `0x0040` | Link S88 |
| `0xFFFF` | CS2/CS3 (maestro) |

## 4. Convivencia con la MS2

- El ESP32 puede estar conectado **a la vez que la MS2**: ambos son
  participantes del mismo bus. Las órdenes de uno y otro llegan a la
  Gleisbox.
- El ESP32 **escucha todo el tráfico**: si mueves una loco con la MS2,
  aparece sola en la interfaz web con su dirección (muy útil para averiguar
  el SID de las locos mfx).
- **Locos mfx**: la MS2 es quien las registra y les asigna un SID. Con la
  MS2 conectada se pueden manejar desde el ESP32 con `mfx:<SID>`. Sin MS2,
  usa locos MM2 o DCC (el registro mfx no está implementado en este
  proyecto).
- Si la MS2 tiene seleccionada la misma loco, normalmente refleja los
  cambios que hace el ESP32 (igual que ocurre con dos MS2), pero no se ha
  verificado en todas las versiones de firmware de la MS2.

## 5. Sensores (retroseñalización)

La Gleisbox **no tiene entradas S88**. Opciones:

1. **Sensores conectados al ESP32** (lo más sencillo): contactos reed o
   sensores Hall con imán, barreras infrarrojas, o vías de contacto con un
   optoacoplador. Ver [hardware.md](hardware.md).
2. **Link S88 (60883)** u otros módulos CAN que envían eventos S88
   (comando `0x11`): el firmware los recibe y los trata igual. Numeración:
   contacto `n` del módulo 0 → sensor `n`; del módulo `m` → `m*1000 + n`.

## 6. Proyectos y documentación de referencia

- **Railuino** (Jörg Pleumann): biblioteca Arduino + shield CAN para la
  Gleisbox. Versión para ESP32 de Christophe Bobille:
  <https://github.com/BOBILLEChristophe/Railuino>
- **GBert/railroad** (can2udp, can-monitor, MäCAN...): pasarela CAN↔UDP
  para Rocrail/Win-Digipet y decodificador completo del protocolo:
  <https://github.com/GBert/railroad>
- **Stefan Krauß – "Märklin Gleisbox als Zentrale"**: conexión de un
  interfaz CAN a la Gleisbox y patillajes de los conectores CAN de Märklin:
  <http://www.skrauss.de/modellbahn/gbox.html>,
  <https://www.skrauss.de/modellbahn/canbus_stecker.html>
- **Banana Pi como interfaz CAN de la Gleisbox** (Mittelleiter-Magazin):
  <https://www.mittelleiter-magazin.de/technik/aufbau-eines-bananapi-can-interfaces-zur-nutzung-der-maerklin-gleisbox-als-zentrale/>
- **Stummiforum – "Anschlussbelegung Märklin Gleisbox (60113/60116)"**:
  <https://www.stummiforum.de/t153609f5-Anschlussbelegung-M-rklin-Gleisbox.html>
- **Märklin-Users.net** – hilos sobre el hash del protocolo CAN y Railuino:
  <https://www.marklin-users.net/forum/posts/t44121-The-Hash-field-in-the-CAN-bus-protocol>,
  <https://www.marklin-users.net/forum/posts/t26642-An-alternative-command-station--Railuino>
- **ESP32 TWAI** (documentación de Espressif): controlador CAN integrado,
  solo requiere transceptor externo.

> Nota: el documento oficial del protocolo y algunos foros no estaban
> accesibles directamente durante la investigación; los valores de las
> tablas se han contrastado con el código fuente abierto de can2udp y
> Railuino, que llevan años funcionando con Gleisbox reales.
