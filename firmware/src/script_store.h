// Scripts guardados en LittleFS como /scripts/<nombre>.txt
#pragma once

#include <Arduino.h>

#include <vector>

namespace scripts {

constexpr size_t kMaxScriptSize = 16 * 1024;

// Nombres válidos: 1..31 caracteres a-z, 0-9, _ y -.
bool validName(const String& name);
std::vector<String> list();
bool exists(const String& name);
bool read(const String& name, String* out);
bool write(const String& name, const String& content);
bool remove(const String& name);
// Graba los ejemplos incluidos la primera vez que arranca el equipo.
void installDefaults();

}  // namespace scripts
