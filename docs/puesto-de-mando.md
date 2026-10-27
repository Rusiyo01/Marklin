# Puesto de mando (portátil)

La aplicación está pensada para manejar la maqueta desde el **portátil** (el
móvil sirve como mando secundario: las mismas pantallas se adaptan a una
columna). Tiene tres pestañas para el día a día:

| Pestaña | Para qué |
|---|---|
| **Locomotoras** | Tu inventario a la izquierda y de **1 a 8 locomotoras a la vez**, con sus funciones (luces, sonidos, bocina...). |
| **Desvíos** | El plano de la maqueta para cambiar desvíos y, al lado, **el mando de una loco** que eliges entre las activas. |
| **Inventario** | Fichas de tus locomotoras **con foto**, edición de funciones y **lectura de las locos de la MS2**. |

```
Locomotoras
┌───────────────┬──────────────────────────────────────────────────────┐
│ Mi inventario │ Locomotoras a la vez: 1 2 3 4 5 6 7 8   Parar todas  │
│ [foto] BR 86  ├─────────────┬─────────────┬─────────────┬────────────┤
│ [foto] ICE 3  │  1 BR 86    │  2 ICE 3    │  3 ...      │  4 ...     │
│ [foto] V 100  │  55 %  ▶    │  0 %        │             │            │
│               │  💡 🔊 📯 💨 │  💡 F1 F2.. │             │            │
└───────────────┴─────────────┴─────────────┴─────────────┴────────────┘

Desvíos
┌────────────────────────────────────────────┬─────────────────────────┐
│ entrada recto · salida desviado            │ Locomotora: [BR 86 ▾]   │
│            plano de la maqueta             │  55 %  ▶               │
│   (clic en un desvío = cambiarlo)          │  ◀ − ■ + ▶             │
│                                            │  💡 🔊 📯 💨            │
└────────────────────────────────────────────┴─────────────────────────┘
```

## Conectar el portátil

- **Recomendado:** en el asistente (o en *Ajustes*) une el ESP32 a la **WiFi
  de casa**. El portátil sigue teniendo Internet y la aplicación se abre en
  **http://marklin.local** (o con la IP que aparece en *Ajustes*).
- **Sin WiFi de casa:** conecta el portátil a la red **Marklin-ESP32** y
  abre **http://192.168.4.1**.
- **Como aplicación de escritorio:** en Chrome o Edge, menú **⋮ → Enviar,
  guardar y compartir → Crear acceso directo… → Abrir como ventana**.

## Locomotoras: de 1 a 8 a la vez

- Elige cuántas con **Locomotoras a la vez: 1 … 8**.
- **Clic** en una loco de *Mi inventario* la lleva al primer mando libre (si
  ya está en un mando, lo selecciona). También puedes **arrastrarla** a un
  mando o elegirla en el desplegable de un mando vacío.
- Cada mando tiene: velocidad, regulador (también con la **rueda** del
  ratón), sentido ◀ ▶, −/+ 10 %, parar ■ y las **funciones de esa loco con
  su símbolo**: 💡 luces, 🔊 sonido, 📯 bocina, 🎶 silbato, 🔔 campana, 💨
  humo, 🚂 ruido de marcha, ⚡ pantógrafo... Las que no tienen símbolo están
  en **Otras funciones** (F0–F31).
- Las funciones **momentáneas** (borde discontinuo; p. ej. la bocina)
  suenan **mientras mantienes pulsado** el botón, como en la MS2.
- **✕** deja el mando libre (la loco no se para).

Cada navegador recuerda sus mandos abiertos.

## Desvíos con una loco a mano

- Es la **maqueta activa** (se elige o se sube en la pestaña *Maqueta*, ver
  [maquetas.md](maquetas.md)). **Clic en un desvío** lo cambia; el
  recorrido activo se dibuja en amarillo. Arriba están **los botones de
  todos los desvíos** con su posición.
- **Rueda**: zoom; **arrastrar**: mover; **doble clic** o ⤢: ajustar.
- A la derecha, **Locomotora mientras cambio desvíos**: elige una de las
  **activas** (las que están en los mandos o en marcha aparecen primero) y
  llévala sin cambiar de pestaña.
- Debajo del plano puedes accionar cualquier desvío o señal **por su
  dirección** (p. ej. `5` o `dcc:12`).

## Inventario

Cada locomotora tiene una ficha con **foto**, nombre, dirección, nombre para
los scripts y los símbolos de sus funciones. Desde la ficha: **Conducir**
(la lleva a un mando) y **Editar**.

- **+ Registrar locomotora**: nombre libre, protocolo (MM2, DCC o mfx),
  dirección, foto y funciones.
- **Foto**: en *Editar* → **Elegir foto** (desde el portátil o la cámara del
  móvil). Se reduce sola antes de guardarla en el ESP32 (unos 30 KB por
  foto; caben decenas).
- **Funciones**: para cada F0…F31 eliges qué es (luces, sonido, bocina...) y
  si es **momentánea**. Es lo que se ve en los mandos y lo que reciben las
  apps como RailControl Pro.
- **Detectadas en la vía y sin registrar**: locos que se han movido (con la
  MS2 o una app) y aún no están en el inventario; **Registrar** las añade.

### Leer las locomotoras de la MS2 (como una Central Station)

Cuando pones en la vía una **loco mfx nueva**, la MS2 la da de alta sola,
como siempre: le asigna una dirección y lee del decodificador su **nombre y
sus funciones**. El ESP32 copia después todos esos datos a su inventario,
igual que hace una Central Station con las MS2 conectadas a ella:

- **Automático** (casilla *Copiar solas las locos nuevas de la MS2*, activa
  de fábrica): cuando el ESP32 ve en el bus que la MS2 da de alta una loco
  mfx, unos 20 segundos después le pide sus locos y añade las nuevas con su
  nombre, dirección y funciones (y vuelve a leerlas un poco más tarde por si
  la MS2 aún estaba leyendo el decodificador). También lo hace cuando se
  mueve una loco que no está en el inventario.
- **A mano**: botón **Leer locos de la MS2**. Copia todas las locos guardadas
  en la MS2 (mfx, MM2 y DCC) que aún no estén en el inventario y actualiza
  las funciones de las que ya estaban.

Lo que no viene de la MS2 se conserva: la foto, el nombre que les hayas
puesto y las funciones que hayas configurado a mano en locos MM2/DCC.

> La lectura de locos mfx nuevas **necesita la MS2 conectada** a la
> Gleisbox: es ella quien las da de alta y lee el decodificador. Sin MS2,
> registra la loco a mano (con su dirección mfx de la MS2/CS) y elige sus
> funciones en *Editar*.

## Teclado

En **Locomotoras** actúa sobre el **mando activo** (borde amarillo); en
**Desvíos**, sobre la loco elegida a la derecha:

| Tecla | Acción |
|---|---|
| Tab / Mayús+Tab | Siguiente / anterior mando (Locomotoras) |
| ↑ / ↓ | Velocidad ±5 % |
| RePág / AvPág | Velocidad ±20 % |
| → / ← | Sentido adelante / atrás |
| Espacio | Parar la loco |
| 0 – 9 | Funciones F0 – F9 |
| Esc | **STOP**: corta la corriente de vía |
