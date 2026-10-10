// Estado global de la aplicación y acciones compartidas por la interfaz web
// y la consola serie.
#pragma once

#include <Arduino.h>

#include "app_config.h"
#include "automation.h"
#include "can_twai.h"
#include "marklin_controller.h"

extern AppConfig config;
extern TwaiBus canBus;
extern marklin::Controller controller;
extern automation::Engine engine;

// Inicializa todo (llamar desde setup()).
void appSetup();
// Bucle principal (llamar desde loop()).
void appLoop();

// Parada de emergencia: corta la corriente de vía y detiene los scripts.
void emergencyStop(const char* reason);

// Arranca un script guardado.
bool startScript(const String& name, String* error);

// Compila un script (resolviendo los nombres globales de la configuración)
// sin ejecutarlo, para validar desde el editor.
automation::Program compileScript(const char* source);

// Nombre configurado de la loco o su dirección ("mm2:24").
String locoLabel(uint32_t uid);
// Nombre configurado del sensor o "".
String sensorName(uint32_t id);

// Reinicia el ESP32 dentro de `delayMs` (para responder antes a la web).
void scheduleRestart(uint32_t delayMs);
