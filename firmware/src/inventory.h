// Inventario de locomotoras: fotos y lectura de las locos de la MS2.
//
// - Fotos: una por loco en /photos/<uid>.jpg (la web las reduce antes de
//   subirlas).
// - Importación: copia de la MS2 nombre, dirección y tipo de cada función
//   de sus locos. Se lanza a mano o sola cuando la MS2 da de alta una loco
//   mfx nueva o cuando se mueve una loco que aún no está registrada.
#pragma once

#include <Arduino.h>

#include "marklin_can.h"

namespace inventory {

void setup();
void loop(uint32_t nowMs);
// Llamar con cada trama del bus.
void onBusFrame(const marklin::CanFrame& f, bool tx);

// Lectura de la MS2.
bool startImport(String* error);
bool importBusy();
// Texto del estado de la última lectura ("" si no ha habido ninguna).
String importStatus();

// Cambia cada vez que se modifica el inventario (para que la web lo recargue).
uint32_t seq();
void touch();

// Fotos.
String photoPath(uint32_t uid);
bool hasPhoto(uint32_t uid);
void removePhoto(uint32_t uid);
void renamePhoto(uint32_t from, uint32_t to);

// Nombre para scripts a partir de un nombre libre ("BR 86 DB" -> "br_86_db"),
// sin repetir los ya registrados.
std::string scriptName(const std::string& label);

}  // namespace inventory
