// Maquetas importadas de "C-Track Layout Planner" (/layouts/<nombre>.json).
//
// El fichero es el mismo JSON que exporta el planificador, más una sección
// "marklin" que añade la web del ESP32 con la asignación de desvíos y
// sensores a piezas de la maqueta:
//   "marklin": {
//     "turnouts": [{"piece": 12, "name": "entrada", "addr": "mm:1", "addr2": "mm:2", "invert": false}],
//     "sensors":  [{"piece": 30, "id": 3, "name": "anden"}]
//   }
// Los nombres de la maqueta activa se pueden usar en los scripts.
#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

namespace layouts {

constexpr size_t kMaxLayoutSize = 48 * 1024;

// Sin inicializadores por defecto para poder crearlas con llaves en C++11.
struct NamedAccessory {
  std::string name;
  uint32_t uid;
};
struct NamedSensor {
  std::string name;
  uint32_t id;
};

bool validName(const String& name);
std::vector<String> list();
bool exists(const String& name);
bool read(const String& name, String* out);
// Valida que sea una maqueta (JSON con "pieces") y la guarda.
bool write(const String& name, const String& json, String* error);
bool remove(const String& name);
// Título ("name" dentro del JSON) de una maqueta guardada.
String title(const String& name);
// Graba las maquetas de ejemplo la primera vez que arranca el equipo.
void installDefaults();

// Carga los nombres de desvíos y sensores de la maqueta activa.
void loadActive(const std::string& name);
const std::vector<NamedAccessory>& accessories();
const std::vector<NamedSensor>& sensors();

}  // namespace layouts
