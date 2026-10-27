# Programar decodificadores

Pestaña **Program** (*Programar* en español): lee y escribe CV a través de la
Gleisbox, como la MS2/CS3.

| Modo | Leer | Escribir | En la vía |
|---|---|---|---|
| DCC · programación | sí | sí | **solo** la loco a programar |
| DCC · vía principal (POM) | no | sí | puede haber más locos (se usa su dirección) |
| MM2 programable (mSD/mLD, ESU...) | no | sí | **solo** la loco a programar |
| mfx (SID de la MS2) | sí | sí | normal; CV + índice según el decodificador |

- **Valores habituales**: CV 1–8, 17, 18 y 29 (DCC) o CV 1–5 y 8 (MM2), con
  botones Leer/Escribir. Escribir 8 en CV 8 restablece el decodificador.
- **Cualquier CV**: CV 1–1023 (e índice 0–63 en mfx), valor 0–255.
- **Cambiar dirección**: DCC corta (1–127, CV 1) o larga (128–10239, CV 17/18),
  ajustando el bit 5 de CV 29; MM2 en CV 1. En mfx la dirección la asigna la MS2.
- El desplegable del inventario copia modo y dirección de una loco registrada.
- **Resultados** muestra cada operación: *escrito y comprobado*, *escrito*,
  *sin confirmar* o *sin respuesta*.

API: `POST /api/cv?loco=dcc:3&cv=29&op=read` (o `op=write&value=6`, `main=1`
para POM, `index=` en mfx) y `GET /api/cv` con el último resultado (`seq`
cambia con cada respuesta). Protocolo CAN: órdenes 0x07 (leer) y 0x08
(escribir).

> No probado aún con hardware real. La 60116 no tiene vía de programación
> aparte: en los modos de programación deja una sola loco en la vía.
