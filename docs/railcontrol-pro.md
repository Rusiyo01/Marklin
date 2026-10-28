# iPad con RailControl Pro (el ESP32 como Central Station)

El ESP32 se presenta en la red como una **Märklin Central Station**: habla
el mismo protocolo de red que la CS2/CS3, así que las apps hechas para la
Central Station se conectan a él como si fuera una. La principal es
**RailControl Pro** (iPad, iPhone y Mac). La app es de un tercero y se
compra en la App Store; este proyecto no la incluye ni la copia, solo
hace que el ESP32 sepa hablar con ella.

```
 iPad (RailControl Pro) ──WiFi── ESP32 ──bus CAN── Gleisbox ── vía
                                  │                   │
                         portátil / móvil            MS2
```

## Cómo conectarlo

1. Instala la versión **1.3.0** o posterior del firmware.
2. En el iPad, ve a **Ajustes → WiFi** y conéctate a **`Marklin-ESP32`**
   (contraseña `marklin123`). Si avisa de que la red no tiene Internet, elige
   **mantener la conexión**.
   - Si el ESP32 ya está unido a tu WiFi de casa, también puedes dejar el iPad
     en la WiFi de casa y usar la IP de casa del ESP32.
3. Abre **RailControl Pro**, añade una central del tipo **Märklin Central
   Station** y, si no la encuentra sola, escribe la IP a mano:
   - en la red `Marklin-ESP32`: **`192.168.4.1`**;
   - en la WiFi de casa: la que aparece en la web del ESP32 → **Ajustes →
     iPad · RailControl Pro** (por ejemplo `192.168.1.57`).
4. La app pide a la central su configuración y recibe:
   - **las locomotoras del inventario** del ESP32 (con su nombre, dirección,
     protocolo MM2, DCC o mfx y el símbolo de cada función: luces, sonido,
     bocina...);
   - **los desvíos de la maqueta activa** (con su nombre y dirección);
   - un **plano de vías** sencillo: un panel con todos los desvíos y
     sensores para accionarlos con un toque.

En la web del ESP32, **Ajustes → iPad · RailControl Pro** muestra cuántas
apps hay conectadas y cuántas órdenes han mandado. En **Monitor** verás las
órdenes de la app como tramas CAN.

## Qué funciona

| | |
|---|---|
| Velocidad, sentido y funciones de las locos | Sí: las órdenes de la app van al bus CAN como si vinieran de una CS2. |
| Corriente de vía (GO/STOP) | Sí. |
| Desvíos y señales | Sí, con la dirección del accesorio (MM o DCC). |
| Lista de locos y desvíos | Sí: las locos registradas en el ESP32 y los desvíos de la maqueta activa. |
| Sensores | Los de la Gleisbox/Link S88 por CAN y los sensores en GPIO del ESP32 (se envían a la app como eventos S88). |
| Varios mandos a la vez | Sí: el iPad, la web del portátil/móvil, los scripts y la MS2 pueden usarse a la vez; todos ven los cambios de los demás. Hasta 4 apps conectadas a la vez. |
| Plano de vías dibujado con la forma real | No: la app recibe un panel con los desvíos. El plano completo está en la web del portátil. |
| Itinerarios (*Fahrstraßen*) de la central | No: se envía la lista vacía. Usa los scripts del ESP32 o los de la propia app. |
| Funciones exclusivas de la CS3 por su web | No: el ESP32 se identifica como **CS2** (versión 4.2), que RailControl Pro maneja por completo por CAN. |

## Si no conecta

- Comprueba que el iPad está en la misma red que el ESP32 (red
  `Marklin-ESP32` o la de casa) y escribe la IP a mano.
- En el iPad, **desactiva los datos móviles** mientras usas la red
  `Marklin-ESP32` si la app intenta salir por ellos (esa red no tiene
  Internet).
- Si la app no ve las locos: regístralas en la web del ESP32 (**Puesto de
  mando → Registrar locomotora**) y vuelve a conectar la app, que pide la
  lista al conectarse.
- Si no ve los desvíos: activa una maqueta y asigna las direcciones a sus
  desvíos (**Maqueta → Desvíos y sensores de esta maqueta**).
- Mira el **Monitor**: deben aparecer líneas `Red CS2: app conectada...` y
  `Red CS2: la app pide 'loks'`.

> No se ha podido probar con la app real durante el desarrollo (es una app
> de pago del iPad). El protocolo es el documentado y usado por otros
> proyectos que imitan a la CS2 (can2lan, de Gerhard Bertelsmann); si algo
> no funciona con tu versión de la app, el **Monitor** del ESP32 muestra
> exactamente qué pide la app.

## Detalles técnicos

- **TCP 15731**: conexión permanente con cada app. Cada trama CAN viaja en
  13 bytes: identificador (4 bytes, *big endian*), DLC y 8 bytes de datos.
- **UDP**: la app envía al puerto **15731** y el ESP32 responde al **15730**.
  Las búsquedas automáticas (un *ping* por difusión) también se contestan.
- **Ping**: el ESP32 responde como Central Station (tipo de equipo `0xFFFF`,
  versión 4.2) y deja pasar el ping al bus para que la app vea también la
  Gleisbox y la MS2. También contesta a las apps que, en vez de preguntar,
  se anuncian con su propia respuesta de ping (tipo `0xEEEE`, "CS2
  software"), y cada 5 s difunde su respuesta de ping al puerto UDP 15730
  para las apps con búsqueda automática.
- **Ficheros de configuración** (orden CAN `0x20`/`0x21`): `loks` →
  `lokomotive.cs2`, `mags` → `magnetartikel.cs2`, `gbs` → `gleisbild.cs2`,
  `gbs-0` → página del plano, `fs` → `fahrstrassen.cs2`, y los de estado
  (`lokstat`, `magstat`, `gbsstat`, `fsstat`). Se envían como en la CS2:
  primera trama con longitud y CRC-CCITT, y después el fichero comprimido
  con zlib en tramas de 8 bytes.
- Los mismos ficheros se pueden descargar por HTTP, como en la CS2:
  `http://192.168.4.1/config/lokomotive.cs2`, `magnetartikel.cs2`,
  `gleisbild.cs2` y `gleisbilder/<página>.cs2`.
- Código: [`firmware/lib/cs2/cs2_lan.cpp`](../firmware/lib/cs2/cs2_lan.cpp)
  (protocolo y ficheros, con tests en el PC) y
  [`firmware/src/cs2_server.cpp`](../firmware/src/cs2_server.cpp) (sockets).
