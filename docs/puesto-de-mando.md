# Puesto de mando (portátil)

El **Puesto de mando** es la pantalla principal. Está pensado para manejar
la maqueta desde el **portátil** (el móvil sirve como mando secundario: la
misma pantalla se adapta a una columna).

```
┌───────────────┬───────────────────────────────────────────────────────┐
│ Locomotoras   │ [Plano y mandos | Solo mandos | Solo plano] Mandos 1 2 3 4│
│ ───────────── ├───────────────────────────────────────────────────────┤
│ Cocodrilo SBB │                 plano de la maqueta                   │
│ ICE 3         │      (desvíos con un clic, sensores en verde,         │
│ BR 151        │       rueda = zoom, arrastrar = mover)                │
│ + Registrar   ├─────────────┬─────────────┬─────────────┬─────────────┤
│               │  Mando 1    │  Mando 2    │  Mando 3    │  Mando 4    │
└───────────────┴─────────────┴─────────────┴─────────────┴─────────────┘
```

## Conectar el portátil

- **Recomendado:** en el asistente (o en *Ajustes*) une el ESP32 a la **WiFi
  de casa**. El portátil sigue teniendo Internet y la aplicación se abre en
  **http://marklin.local**.
- **Sin WiFi de casa:** conecta el portátil a la red **Marklin-ESP32** y
  abre **http://192.168.4.1**. Mientras tanto ese portátil no tendrá
  Internet.
- **Como aplicación de escritorio:** en Chrome o Edge, menú **⋮ → Enviar,
  guardar y compartir → Crear acceso directo… → Abrir como ventana**.
  Aparece un icono "Märklin" en el escritorio que la abre en su propia
  ventana, sin barras del navegador.

## Locomotoras registradas

La columna de la izquierda es tu **registro de locomotoras**:

- **+ Registrar**: nombre libre (p. ej. *Cocodrilo SBB*), protocolo (MM2,
  DCC o mfx) y dirección. El "nombre en scripts" se genera solo
  (`cocodrilo_sbb`) y es el que usas en los scripts.
- **Detectadas**: las locos que muevas con la MS2 aparecen solas con su
  dirección; pulsa **Registrar** para añadirlas con nombre.
- **✎** edita o elimina una loco registrada.
- La barra amarilla bajo cada loco indica su velocidad actual, y el número
  en el círculo, en qué mando está.

El registro se guarda en el ESP32: es el mismo en el portátil y en el móvil.

## De 1 a 4 mandos a la vez

- Elige cuántos mandos quieres con **Mandos: 1 2 3 4** (pantalla dividida).
- **Clic** en una locomotora de la lista la lleva al primer mando libre (si
  ya está en un mando, lo selecciona). También puedes **arrastrarla** a un
  mando concreto o elegirla en el desplegable de un mando vacío.
- Cada mando tiene: velocidad grande, regulador (también con la **rueda**
  del ratón), sentido ◀ ▶, −/+ 10 %, parar ■ y funciones F0–F31.
- **✕** deja el mando libre (la loco no se para).
- Vistas: **Plano y mandos**, **Solo mandos** (mandos más grandes, 4 en
  cuadrícula 2×2) y **Solo plano**.

Cada navegador recuerda sus mandos abiertos y su vista.

## Teclado

Actúa sobre el **mando activo** (borde amarillo):

| Tecla | Acción |
|---|---|
| Tab / Mayús+Tab | Siguiente / anterior mando |
| ↑ / ↓ | Velocidad ±5 % |
| RePág / AvPág | Velocidad ±20 % |
| → / ← | Sentido adelante / atrás |
| Espacio | Parar la loco |
| 0 – 9 | Funciones F0 – F9 |
| Esc | **STOP**: corta la corriente de vía |

## Plano y desvíos

- Es la **maqueta activa** (se elige o se sube en la pestaña *Maqueta*, ver
  [maquetas.md](maquetas.md)).
- **Clic en un desvío** lo cambia; el recorrido activo se dibuja en
  amarillo. Arriba a la derecha hay **botones con todos los desvíos** y su
  posición, por si el plano es grande.
- **Rueda** del ratón: zoom; **arrastrar**: mover; **doble clic** o ⤢:
  ajustar.
- Los **sensores** ocupados se iluminan en verde.
