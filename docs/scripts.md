# Lenguaje de scripts

Los scripts describen lo que deben hacer los trenes, línea a línea. Se
escriben y ejecutan desde la pestaña **Automático** de la interfaz web (o
con `run <nombre>` en la consola serie) y se guardan en la memoria del
ESP32.

```
# Lanzadera sencilla
loco tren mm2 24
sensor fin_a 1
sensor fin_b 2

power on
label inicio
  dir tren fwd
  ramp tren 50% 3s
  wait sensor fin_b on timeout 2m
  ramp tren 0 1s
  wait 10s
  dir tren rev
  ramp tren 50% 3s
  wait sensor fin_a on timeout 2m
  ramp tren 0 1s
  wait 10s
goto inicio
```

## Conceptos

- **Una orden por línea.** Lo que va detrás de `#` o `//` es un comentario.
  Mayúsculas y minúsculas dan igual.
- **Varios scripts a la vez**: hasta **4** en paralelo (por ejemplo, uno por
  tren). Se coordinan con `lock`/`unlock`.
- **Seguridad**: si un script se **detiene** (botón Detener, parada de
  emergencia) o termina con **error** (por ejemplo, un `timeout`), todas las
  locos que ese script ha manejado se ponen a velocidad 0 y se liberan sus
  bloqueos. Si termina normalmente (fin del texto o `exit`), las locos se
  quedan como estén.
- **Comprobación**: el botón *Comprobar* del editor detecta errores (orden
  desconocida, etiqueta inexistente, `repeat` sin `end`...) e indica la
  línea, antes de mover ningún tren.
- Un bucle sin ninguna espera no bloquea el equipo: el motor inserta una
  pausa automática de 50 ms para no saturar el bus.

## Referencias

| Qué | Cómo se escribe | Ejemplos |
|---|---|---|
| Loco | alias definido con `loco`, nombre de la configuración, o protocolo:dirección | `tren`, `mm2:24`, `dcc:3`, `mfx:5` |
| Sensor | alias definido con `sensor`, nombre de la configuración, o número | `anden`, `3`, `2005` |
| Accesorio | número (MM) o protocolo:dirección | `5`, `mm:5`, `dcc:12` |
| Velocidad | 0–1000 o porcentaje | `400`, `40%` |
| Duración | número + unidad (`ms`, `s`, `m`, `h`); sin unidad = segundos | `500ms`, `2s`, `1.5s`, `2m`, `3` |
| Estado de sensor | `on` / `off` (también `1`/`0`, `ocupado`/`libre`) | |

Números de sensor: los locales (GPIO) llevan el número que les des en la
configuración; los que llegan por CAN (Link S88) son `contacto` (módulo 0) o
`módulo*1000 + contacto`.

## Órdenes

### Definiciones

| Orden | Descripción |
|---|---|
| `loco <alias> <protocolo> <dirección>` | Define un alias de loco: `loco cocodrilo mm2 24`, `loco ice dcc 3`, `loco br89 mfx 5`. También `loco x mm2:24`. |
| `sensor <alias> <número>` | Define un alias de sensor: `sensor anden 3`. |

### Corriente y locos

| Orden | Descripción |
|---|---|
| `power on` / `power off` | Da o corta la corriente de vía (GO / STOP). `go` equivale a `power on`. |
| `speed <loco> <velocidad>` | Fija la velocidad al instante. `speed tren 0` para. |
| `ramp <loco> <velocidad> <duración>` | Cambia la velocidad de forma gradual desde la actual: `ramp tren 60% 3s` acelera, `ramp tren 0 2s` frena. El script espera a que termine la rampa. |
| `dir <loco> fwd` / `rev` / `toggle` | Sentido de marcha (también `adelante`, `atras`, `cambiar`). **Deja la loco parada.** |
| `func <loco> <n> on` / `off` | Función F0–F31 (`func tren 0 on` = luces; también `f0`). |
| `estop <loco>` | Parada de emergencia de una loco. |
| `halt` | Pone a 0 todas las locos conocidas (sin cortar la vía). |

### Desvíos y señales

| Orden | Descripción |
|---|---|
| `switch <accesorio> straight` | Recto / verde (también `recto`, `green`, `verde`, `1`). |
| `switch <accesorio> round` | Desviado / rojo (también `desviado`, `curva`, `red`, `rojo`, `0`). |
| `switch <accesorio> yellow` / `white` | Tercera y cuarta salida (desvío triple, señales). |

`turnout`, `signal`, `desvio` y `senal` son sinónimos de `switch`. La
bobina se desactiva sola tras `can.accessoryPulseMs` (250 ms por defecto).

### Esperas

| Orden | Descripción |
|---|---|
| `wait <duración>` | Espera un tiempo: `wait 10s`. |
| `wait random <mín> <máx>` | Espera un tiempo aleatorio: `wait random 10s 45s`. |
| `wait sensor <s> on` / `off` | Espera a que el sensor esté activo/libre. Si ya lo está, sigue enseguida. Detecta también pulsos cortos (contactos reed) ocurridos durante la espera. |
| `wait sensor <s> on timeout <duración>` | Igual, pero si pasa el tiempo **el script se detiene con error** y para sus locos. |
| `wait sensor <s> on timeout <duración> else <etiqueta>` | Si pasa el tiempo, salta a la etiqueta en lugar de dar error. |

> Consejo: pon siempre `timeout` en las esperas de sensores. Si un sensor
> falla o el tren descarrila, el script se detiene en vez de dejar la loco
> en marcha para siempre.

### Control del flujo

| Orden | Descripción |
|---|---|
| `label <nombre>` | Marca un punto del script. |
| `goto <nombre>` | Salta a la etiqueta. |
| `if sensor <s> on goto <etiqueta>` | Salta si el sensor está en ese estado. |
| `if chance <1-100> goto <etiqueta>` | Salta con esa probabilidad (en %). |
| `if locked <bloqueo> goto <etiqueta>` | Salta si **otro** script tiene ese bloqueo. |
| `repeat <n>` … `end` | Repite el bloque n veces. Se pueden anidar. |
| `repeat` … `end` | Repite para siempre (sal con `goto`). |
| `exit` | Termina el script. |

### Coordinación entre scripts

| Orden | Descripción |
|---|---|
| `lock <nombre>` | Reserva un recurso (un tramo de vía, un puente, un cruce). Si lo tiene otro script, **espera** a que lo libere. |
| `unlock <nombre>` | Lo libera. |

Los bloqueos se liberan automáticamente cuando el script termina o se
detiene.

### Otros

| Orden | Descripción |
|---|---|
| `log <texto>` | Escribe un mensaje en el registro (pestaña Monitor y "Mensajes de los scripts"). |

## Ejemplos

Al arrancar por primera vez el ESP32 se instalan estos ejemplos (puedes
editarlos o borrarlos):

| Script | Qué hace |
|---|---|
| `demo_tiempo` | Ida y vuelta solo con tiempos (sin sensores). Ideal para la primera prueba. |
| `lanzadera` | Lanzadera entre dos estaciones con un sensor en cada extremo y paradas de duración aleatoria. |
| `puente_tren1` + `puente_tren2` | Dos trenes en circuitos distintos que comparten un puente de vía única, coordinados con `lock`. |
| `estacion_aleatoria` | Circuito con estación de dos vías; el tren elige vía al azar con un desvío. |

### Patrón: tramo compartido entre dos trenes

```
label vuelta
  wait sensor antes_tramo on
  if locked tramo goto esperar      # ¿lo tiene el otro tren?
  lock tramo                        # libre: lo reservo y sigo sin parar
  goto cruzar
label esperar
  ramp tren 0 1s                    # ocupado: freno...
  lock tramo                        # ...y espero a que quede libre
  ramp tren 45% 3s
label cruzar
  wait sensor despues_tramo on timeout 1m
  unlock tramo
goto vuelta
```

### Patrón: horario con número de vueltas

```
loco tren dcc 3
sensor paso 4
power on
dir tren fwd
repeat 10
  ramp tren 50% 2s
  wait sensor paso on timeout 3m
end
ramp tren 0 2s
log 10 vueltas completadas
```

## Desde la consola o la API

Las órdenes sin espera (`power`, `speed`, `dir`, `func`, `estop`, `halt`,
`switch`, `log`) se pueden ejecutar sueltas desde la consola serie, el
campo de órdenes de la pestaña Monitor o `POST /api/exec`:

```
speed mm2:24 30%
func dcc:3 0 on
switch 5 round
```
