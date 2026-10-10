// Marklin ESP32: control automático de trenes Märklin a través del bus CAN
// de la Gleisbox (60113/60116) de la Mobile Station 2.
//
// Ver README.md y docs/ para el montaje y el uso.
#include <Arduino.h>

#include "app.h"

void setup() { appSetup(); }

void loop() {
  appLoop();
  delay(1);  // cede la CPU a las tareas del sistema (WiFi, etc.)
}
