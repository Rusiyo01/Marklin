// Configuración persistente (fichero /config.json en LittleFS).
#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#ifndef DEFAULT_ESTOP_PIN
#define DEFAULT_ESTOP_PIN 0  // botón BOOT de la mayoría de placas ESP32
#endif

struct LocoConfig {
  std::string name;  // alias en minúsculas, usable en los scripts
  uint32_t uid = 0;
};

struct SensorConfig {
  uint32_t id = 0;        // número de sensor en los scripts
  std::string name;       // alias opcional
  int pin = -1;           // GPIO local; -1 = solo por CAN (S88) o simulado
  bool activeLow = true;  // contacto a GND (con pull-up interna)
  uint16_t debounceMs = 30;
};

struct AppConfig {
  // WiFi: "ap" crea su propia red; "sta" se une a la red de casa (si falla,
  // vuelve a modo AP para no quedarse sin acceso).
  std::string wifiMode = "ap";
  std::string staSsid;
  std::string staPassword;
  std::string apSsid = "Marklin-ESP32";
  std::string apPassword = "marklin123";
  std::string hostname = "marklin";

  // Bus CAN.
  int canTxPin = 5;
  int canRxPin = 4;
  std::string gleisboxInit = "auto";  // auto | always | never
  uint8_t protocolMask = 0x07;        // bit0 MM2, bit1 mfx, bit2 DCC
  uint16_t accessoryPulseMs = 250;

  // Pulsador de parada de emergencia (a GND). -1 = desactivado.
  int estopPin = DEFAULT_ESTOP_PIN;

  std::vector<LocoConfig> locos;
  std::vector<SensorConfig> sensors;
  std::vector<std::string> autostart;  // scripts que arrancan solos
};

// Carga /config.json. Si no existe o es inválido, deja los valores por
// defecto y devuelve false.
bool loadConfig(AppConfig* cfg, String* error);

// Interpreta un JSON de configuración (sin guardarlo).
bool parseConfig(const String& json, AppConfig* cfg, String* error);

// Serializa la configuración actual a JSON legible.
String configToJson(const AppConfig& cfg);

// Guarda el texto JSON (validado antes con parseConfig).
bool saveConfigJson(const String& json);
