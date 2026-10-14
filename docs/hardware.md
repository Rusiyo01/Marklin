# Montaje del hardware

> ⚠️ **Lee esto entero antes de conectar nada.** El conector de la Gleisbox
> lleva también la alimentación de la MS2 (~18 V). Si llega a un pin del
> ESP32, lo destruye. Identifica las patillas con el multímetro (sección 3).

## 1. Material

| Cantidad | Componente | Notas |
|---|---|---|
| 1 | Placa **ESP32** (ESP32-DevKitC, NodeMCU-32S, WROOM-32...) | También sirven ESP32-S3 y ESP32-C3 (llevan TWAI). |
| 1 | Módulo transceptor CAN **SN65HVD230** (o VP230) | Funciona a **3,3 V**. Evita los módulos TJA1050/MCP2551 de 5 V sin adaptación de nivel. No hace falta MCP2515. |
| 1 | Forma de llegar al bus CAN de la Gleisbox | Ver sección 2. |
| 1 | Cable de 3 hilos (CAN-H, CAN-L, GND), mejor trenzado | Lo más corto posible (< 30 cm). |
| 1 | Fuente USB 5 V para el ESP32 | O un convertidor reductor desde 18 V (sección 5). |
| — | Sensores opcionales | Reed/Hall + imanes, barreras IR, vías de contacto + optoacoplador. |
| 1 | Pulsador de emergencia opcional | El botón BOOT de la placa ya funciona como parada de emergencia. |

## 2. Cómo llegar al bus CAN

El bus CAN está en la toma de la MS2 de la Gleisbox (10 polos). Solo
necesitas **CAN-H, CAN-L y GND**. Opciones, de menos a más invasiva:

1. **Toma libre de la Gleisbox.** Algunas Gleisbox (la 60113) tienen dos
   tomas para MS2; según Stefan Krauß da igual cuál se use. Si la tuya
   tiene una libre, conecta ahí un adaptador. El conector de 10 polos de la
   MS2 no se vende suelto, así que hará falta fabricarlo o recuperarlo.
2. **Sacar el bus por un conector propio** (lo que hace Stefan Krauß): abrir
   la Gleisbox, soldar tres cables a las patillas CAN-H, CAN-L y GND de la
   toma en la placa y llevarlos a un conector nuevo (por ejemplo un Sub-D 9
   con el estándar CiA: **pin 2 = CAN-L, pin 3 = GND, pin 7 = CAN-H**).
   *Pierdes la garantía.*
3. **Derivar del cable de la MS2** (cortar e intercalar un conector). Es lo
   más arriesgado para la MS2; solo si sabes lo que haces.

### Caso recomendado: Gleisbox con dos tomas para MS2 (p. ej. 60116)

La MS2 se queda en una toma y el ESP32 va en la otra; no hay que abrir nada.

1. Consigue un **cable de conexión de MS2 de repuesto** (el conector de 10
   polos no se vende suelto) y córtalo dejando el conector con 30–50 cm de
   cable.
2. Pela los hilos del extremo cortado **sin que se toquen entre sí** y
   enchufa el conector en la toma libre, con la Gleisbox encendida y la MS2
   en la otra toma.
3. Identifica los hilos con el multímetro (sección 3), midiendo en los
   hilos en vez de en las patillas: el de ~18 V, GND, CAN-H y CAN-L.
4. Aísla con cinta o termorretráctil el hilo de ~18 V y los que no uses.
   Une CAN-H, CAN-L y GND al módulo SN65HVD230 (sección 4).

En todos los casos **no conectes nunca la patilla de alimentación (~18 V)**
al circuito del ESP32 (salvo a la entrada de un convertidor reductor, ver
sección 5).

## 3. Identificar las patillas con el multímetro

Los patillajes publicados en foros se contradicen (uno indica *pin 4 =
CAN-H, pin 8 = CAN-L, pin 2 = GND, pin 1 = alimentación*; otro dice lo
contrario para los pines 1 y 2). Compruébalo en **tu** Gleisbox:

1. **Alimentación y masa** (Gleisbox enchufada a su fuente, MS2
   desconectada de esa toma): mide tensión continua entre parejas de
   patillas. Entre la de alimentación y GND verás unos **18–19 V**.
2. **CAN-H y CAN-L** (Gleisbox encendida, con la MS2 conectada en la otra
   toma o en el mismo cable si derivas de él): respecto a GND, ambas líneas
   están en reposo a unos **2,5 V**. Con tráfico (mueve una loco con la MS2)
   el multímetro marca **CAN-H algo por encima de 2,5 V** (2,6–3 V) y
   **CAN-L algo por debajo** (2–2,4 V).
3. **Terminación** (todo **apagado**): mide la resistencia entre CAN-H y
   CAN-L. Unos **60 Ω** = el bus ya está terminado en los dos extremos;
   unos **120 Ω** = solo en un extremo. Lo necesitarás en la sección 4.

Anota el resultado y marca los cables.

## 4. Conexionado

```
        ESP32                     SN65HVD230                   Gleisbox (toma MS2)
   ┌──────────────┐          ┌──────────────────┐
   │ 3V3 ─────────┼──────────┼─ 3V3 / VCC        │
   │ GND ─────────┼────┬─────┼─ GND              │
   │ GPIO 5 (TX) ─┼────┼─────┼─ CTX (D / TXD)    │
   │ GPIO 4 (RX) ─┼────┼─────┼─ CRX (R / RXD)    │
   └──────────────┘    │     │  CANH ────────────┼────────────── CAN-H
                       │     │  CANL ────────────┼────────────── CAN-L
                       │     └──────────────────┘
                       └───────────────────────────────────────── GND   (¡imprescindible!)
                                                       (no conectar la patilla de ~18 V)
```

- Los pines TX/RX se pueden cambiar en la configuración (`can.txPin`,
  `can.rxPin`). Valores por defecto: **TX = GPIO 5, RX = GPIO 4**.
- **La masa común es obligatoria**: sin GND compartido el CAN no funciona.
- **Resistencia de terminación**: casi todos los módulos SN65HVD230 llevan
  una resistencia de 120 Ω entre CANH y CANL (suele estar marcada como R2).
  - Si mediste ~**60 Ω**, el bus ya está terminado: **quita o corta** la
    resistencia del módulo (con un cable corto suele funcionar igualmente,
    pero queda fuera de especificación).
  - Si mediste ~**120 Ω**, deja la del módulo (el ESP32 hace de extremo).
- Cable corto y trenzado entre la Gleisbox y el transceptor.

### Comprobación rápida

1. Graba el firmware y abre la interfaz web → pestaña **Ajustes**.
2. Pulsa **Buscar equipos (ping)**: deberían aparecer la **Gleisbox** y,
   si está conectada, la **MS2**.
3. Mueve una loco con la MS2: en **Monitor** verás las tramas y la loco
   aparecerá en **Mando**.
4. Si el contador **TEC** sube a 128 o más y no aparece nada: nadie
   confirma las tramas del ESP32 → revisa CAN-H/CAN-L (¿invertidos?), GND y
   la velocidad (debe ser 250 kbit/s, ya configurada).

## 5. Alimentación del ESP32

- **Lo más seguro**: un cargador USB de 5 V. Recuerda unir igualmente la
  GND del ESP32 con la GND del bus CAN.
- **Desde la Gleisbox**: con un convertidor reductor (MP1584, LM2596...)
  ajustado a **5 V** antes de conectarlo, entrada a la patilla de ~18 V y
  GND de la toma, salida al pin **5V/VIN** del ESP32. Hazlo solo después de
  haber identificado las patillas con certeza.
- No alimentes el ESP32 desde la vía: la señal de vía es digital y
  bipolar, no una tensión continua.

## 6. Sensores

Los sensores se conectan a GPIO libres del ESP32 y se declaran en la
configuración (`sensors`, ver [configuracion.md](configuracion.md)).

**GPIO recomendados (ESP32-DevKit):** 13, 14, 16, 17, 18, 19, 21, 22, 23,
25, 26, 27, 32, 33. Evita 0, 2, 12 y 15 (arranque), 6–11 (memoria flash),
1 y 3 (puerto serie) y 4/5 (CAN). Los GPIO **34–39** solo son de entrada y
**no tienen pull-up interna** (pon una resistencia externa de 10 kΩ a 3,3 V).

### a) Reed o Hall + imán (recomendado para empezar)

Un contacto reed (o un sensor Hall tipo A3144) en la vía y un imán bajo la
loco o el vagón. Sin contacto eléctrico con la vía.

```
  GPIO ──────┐
             │  contacto reed
  GND  ──────┘          (configuración: "activeLow": true, pull-up interna)
```

Los contactos reed dan pulsos muy cortos: el firmware los cuenta aunque
duren menos que el ciclo de los scripts. Ajusta `debounceMs` (por defecto
30 ms).

### b) Barrera o reflexión infrarroja

Módulos tipo TCRT5000 o barreras IR con salida digital. **Aliméntalos a
3,3 V** (o asegúrate de que su salida no supere 3,3 V). Según el módulo, la
salida es activa a nivel bajo (`"activeLow": true`) o alto (`false`).

### c) Vía de contacto Märklin (raíl aislado) con optoacoplador

En las vías de contacto, las ruedas unen el raíl exterior aislado con el
otro raíl exterior (masa de vía) cuando pasa un tren. **La masa de vía NO es
la GND del ESP32**: la señal de vía es alterna (unos ±18 V). Hay que aislar
con un optoacoplador:

```
  LADO VÍA (aislado del ESP32)                LADO ESP32
                                       PC817
 Raíl central (B) ──►|── 2,2 kΩ ──► ánodo  │  colector ──┬── GPIO
                  1N4148                LED │  fototr.    ├── 10 kΩ ── 3V3
 Raíl aislado ─────────────────────── cátodo│  emisor ──┐ └── 4,7 µF ── GND
 (sección de contacto)                                  └── GND
```

- Lado vía: raíl central → diodo 1N4148 → resistencia 2,2 kΩ → LED del
  PC817 → raíl aislado de la vía de contacto.
- Lado ESP32: colector del fototransistor al GPIO, con resistencia de
  10 kΩ a 3,3 V y **condensador de 4,7 µF entre el GPIO y GND**; emisor a
  GND.

Cuando hay un tren, circula corriente por el LED en medio ciclo de la señal
de vía; el condensador mantiene la entrada a nivel bajo mientras dure la
ocupación. Configuración: `"activeLow": true`.

### d) Retromódulos por CAN (Link S88)

Si tienes un Link S88 u otro módulo que envíe eventos S88 por CAN, no hay
que cablear nada al ESP32: los eventos llegan solos. Numeración: contacto
`n` del módulo 0 → sensor `n`; del módulo `m` → sensor `m*1000 + n`.

## 7. Parada de emergencia

- El botón **BOOT** de la placa (GPIO 0; GPIO 9 en ESP32-C3) ya funciona
  como **parada de emergencia**: corta la corriente de vía, detiene los
  scripts y pone las locos a velocidad 0.
- Puedes añadir una seta de emergencia entre el GPIO configurado en
  `estopPin` y GND.
- La interfaz web también tiene un botón **EMERGENCIA** y la consola
  serie la orden `!`. Y siempre puedes pulsar STOP en la MS2.
