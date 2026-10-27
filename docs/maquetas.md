# Maquetas de C-Track Planner

Las maquetas que diseñas en **C-Track Planner** (el planificador de vías C
que hicimos en el proyecto "marklin app") se pueden subir al ESP32. La
pestaña **Maqueta** dibuja el plano y lo convierte en un **cuadro de mando**:

- Tocas un desvío y cambia (se dibuja en amarillo el recorrido activo).
- Los sensores ocupados se iluminan en verde en directo.
- Los **nombres** de desvíos y sensores se usan en los scripts:
  `switch entry round`, `wait sensor platform on`.

## Subir una maqueta

1. En C-Track Planner pulsa **Guardar** y después, en *Maquetas*,
   **Exportar archivo de maqueta (.json)**. También vale la **app sin
   conexión (.html)**: el ESP32 extrae la maqueta que lleva dentro.
2. En la web del ESP32, pestaña **Maqueta → Subir maqueta (C-Track
   Planner)** y elige el fichero. (También desde el paso 3 del asistente.)
3. Queda como **maqueta activa**: sus nombres ya se pueden usar en los
   scripts.

Se reconocen todas las piezas del planificador: rectas, curvas R1–R9,
desvíos (normales, esbeltos, curvos), desvío triple, cruces, travesía doble,
vía de alimentación y topera. La imagen de calco no se sube (no hace falta y
ocupa mucho). Tamaño máximo: 48 KB por maqueta (unas 400 piezas).

Si vuelves a subir una maqueta con el **mismo nombre** (por ejemplo, tras
ampliarla en el planificador), se conservan las asignaciones de las piezas
que sigan existiendo.

## Asignar desvíos y sensores

Pulsa **Asignar desvíos y sensores** y toca una pieza:

- **Desvío**: nombre (p. ej. `entrada`), protocolo (MM o DCC) y dirección
  del decodificador. Marca **Invertido** si al pedir "recto" se pone
  desviado. El desvío **triple** usa dos direcciones (una por cada
  accionamiento); en los scripts la segunda se llama `<nombre>_2`.
- **Sensor**: número de sensor (el de la configuración para sensores en
  GPIO, o `contacto` / `módulo*1000 + contacto` para un Link S88) y un
  nombre opcional.

**Numerar desvíos automáticamente** da direcciones MM consecutivas (1, 2,
3…) y nombres `d1`, `d2`… a los desvíos que aún no tengan. Al terminar
pulsa **Guardar asignaciones**.

## Maquetas de ejemplo incluidas

| Maqueta | Piezas | Asignaciones | Script que la usa |
|---|---|---|---|
| `shuttle` | recta con toperas | sensores 1 `station_a` y 2 `station_b` | `shuttle` |
| `station` | óvalo R2 con estación de dos vías | desvíos `entry` (mm:1) y `exit` (mm:2); sensores 7 `arrival`, 8 `platform`, 9 `platform_track2` | `random_station` |
| `branch_line` | el ejemplo del planificador: óvalo con ramal en rampa | desvío `branch` (mm:3); sensor 10 `branch_end` | — |

Se pueden abrir también en C-Track Planner (son ficheros normales de
maqueta) para modificarlas.

## Formato (para curiosos)

Es el mismo JSON que exporta el planificador
(`{"app":"ctrack-planner","v":1,"pieces":[{"id","t","x","y","r","z"}],...}`),
con una sección extra que añade el ESP32:

```json
"marklin": {
  "turnouts": [{"piece": 2, "name": "entrada", "addr": "mm:1", "invert": false}],
  "sensors":  [{"piece": 1, "id": 7, "name": "llegada"}]
}
```

El planificador ignora esa sección, así que el fichero sigue abriéndose en
él.
