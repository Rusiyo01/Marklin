#include "app.h"

#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_random.h>

#include <vector>

#include "console.h"
#include "event_log.h"
#include "script_store.h"
#include "web_ui.h"

AppConfig config;
TwaiBus canBus;
marklin::Controller controller(&canBus);

namespace {

// Puente entre el motor de scripts y el controlador del bus.
class EspHost : public automation::Host {
 public:
  uint32_t nowMs() override { return millis(); }
  void setTrackPower(bool on) override { controller.setTrackPower(on); }
  void setSpeed(uint32_t loco, uint16_t speed) override { controller.setSpeed(loco, speed); }
  uint16_t getSpeed(uint32_t loco) override {
    const marklin::LocoState* l = controller.findLoco(loco);
    return l ? l->speed : 0;
  }
  void setDirection(uint32_t loco, uint8_t dir) override {
    controller.setDirection(loco, static_cast<marklin::Direction>(dir));
  }
  void setFunction(uint32_t loco, uint8_t fn, bool on) override {
    controller.setFunction(loco, fn, on);
  }
  void emergencyStop(uint32_t loco) override { controller.emergencyStop(loco); }
  void stopAllLocos() override { controller.stopAllLocos(); }
  void switchAccessory(uint32_t acc, uint8_t pos) override {
    controller.switchAccessory(acc, pos, config.accessoryPulseMs);
  }
  int8_t sensorState(uint32_t id) override { return controller.sensorState(id); }
  uint32_t sensorEdges(uint32_t id, bool rising) override {
    const marklin::SensorState* s = controller.findSensor(id);
    return s ? (rising ? s->onCount : s->offCount) : 0;
  }
  uint32_t random(uint32_t lo, uint32_t hi) override {
    if (hi <= lo) return lo;
    return lo + esp_random() % (hi - lo + 1);
  }
  void log(const std::string& script, const std::string& msg) override {
    eventLog.addf(LogKind::Script, "[%s] %s", script.c_str(), msg.c_str());
  }
  bool resolveLoco(const std::string& name, uint32_t* uid) override {
    for (const auto& l : config.locos)
      if (l.name == name) {
        *uid = l.uid;
        return true;
      }
    return false;
  }
  bool resolveSensor(const std::string& name, uint32_t* id) override {
    for (const auto& s : config.sensors)
      if (!s.name.empty() && s.name == name) {
        *id = s.id;
        return true;
      }
    return false;
  }
};

EspHost host;

struct LocalSensor {
  uint32_t id;
  int pin;
  bool activeLow;
  uint16_t debounceMs;
  bool stable;
  bool raw;
  uint32_t rawSinceMs;
};
std::vector<LocalSensor> localSensors;

bool estopRaw = false;
bool estopStable = false;
uint32_t estopSinceMs = 0;

uint32_t bootMs = 0;
bool gleisboxDone = false;
bool autostartDone = false;
uint32_t restartAtMs = 0;

void logFrame(const marklin::CanFrame& f, bool tx) {
  char text[96];
  marklin::describeFrame(f, text, sizeof(text));
  eventLog.add(tx ? LogKind::CanTx : LogKind::CanRx, text);
}

void setupLocalSensors() {
  for (const auto& s : config.sensors) {
    controller.declareSensor(s.id);
    if (s.pin < 0) continue;
    pinMode(s.pin, s.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
    LocalSensor ls{s.id, s.pin, s.activeLow, s.debounceMs, false, false, 0};
    ls.raw = ls.stable = (digitalRead(s.pin) == LOW) == s.activeLow;
    controller.setSensor(s.id, ls.stable);
    localSensors.push_back(ls);
  }
  if (!localSensors.empty())
    eventLog.addf(LogKind::Info, "%u sensores locales (GPIO)",
                  static_cast<unsigned>(localSensors.size()));
}

void pollLocalSensors(uint32_t now) {
  for (auto& s : localSensors) {
    const bool active = (digitalRead(s.pin) == LOW) == s.activeLow;
    if (active != s.raw) {
      s.raw = active;
      s.rawSinceMs = now;
    }
    if (s.raw != s.stable && now - s.rawSinceMs >= s.debounceMs) {
      s.stable = s.raw;
      controller.setSensor(s.id, s.stable);
    }
  }
}

void pollEstop(uint32_t now) {
  if (config.estopPin < 0) return;
  const bool pressed = digitalRead(config.estopPin) == LOW;
  if (pressed != estopRaw) {
    estopRaw = pressed;
    estopSinceMs = now;
  }
  if (estopRaw != estopStable && now - estopSinceMs >= 30) {
    estopStable = estopRaw;
    if (estopStable) emergencyStop("pulsador de emergencia");
  }
}

void setupWifi() {
  WiFi.persistent(false);
  WiFi.setHostname(config.hostname.c_str());
  bool connected = false;
  if (config.wifiMode == "sta") {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.staSsid.c_str(), config.staPassword.c_str());
    eventLog.addf(LogKind::Info, "WiFi: conectando a '%s'...", config.staSsid.c_str());
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(100);
    connected = WiFi.status() == WL_CONNECTED;
    if (connected) {
      WiFi.setAutoReconnect(true);
      eventLog.addf(LogKind::Info, "WiFi: conectado, IP %s", WiFi.localIP().toString().c_str());
    } else {
      eventLog.add(LogKind::Error, "WiFi: no se pudo conectar, se activa el punto de acceso");
    }
  }
  if (!connected) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.apSsid.c_str(),
                config.apPassword.empty() ? nullptr : config.apPassword.c_str());
    eventLog.addf(LogKind::Info, "WiFi: punto de acceso '%s', IP %s", config.apSsid.c_str(),
                  WiFi.softAPIP().toString().c_str());
  }
  WiFi.setSleep(false);
  if (MDNS.begin(config.hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    eventLog.addf(LogKind::Info, "http://%s.local/", config.hostname.c_str());
  }
}

// Arranque: decidir si hay que despertar a la Gleisbox y lanzar los
// scripts de inicio automático.
void bootSequence(uint32_t now) {
  if (!gleisboxDone && now - bootMs >= 1500) {
    gleisboxDone = true;
    if (config.gleisboxInit == "always" ||
        (config.gleisboxInit == "auto" && !controller.ms2Present())) {
      controller.startGleisbox(config.protocolMask);
      eventLog.add(LogKind::Info, "Gleisbox: enviada secuencia de arranque (sin MS2 detectada)");
    } else if (controller.ms2Present()) {
      eventLog.add(LogKind::Info, "MS2 detectada en el bus: trabajando junto a ella");
    }
    controller.queryTrackPower();
  }
  if (!autostartDone && now - bootMs >= 2500) {
    autostartDone = true;
    for (const auto& name : config.autostart) {
      String err;
      if (!startScript(name.c_str(), &err))
        eventLog.addf(LogKind::Error, "autoarranque '%s': %s", name.c_str(), err.c_str());
    }
  }
}

}  // namespace

automation::Engine engine(&host);

void emergencyStop(const char* reason) {
  controller.setTrackPower(false);
  engine.stopAll();
  controller.stopAllLocos();
  eventLog.addf(LogKind::Error, "PARADA DE EMERGENCIA: %s", reason);
}

bool startScript(const String& name, String* error) {
  String src;
  if (!scripts::read(name, &src)) {
    *error = "no existe el script '" + name + "'";
    return false;
  }
  std::string err;
  if (engine.start(name.c_str(), src.c_str(), &err) < 0) {
    *error = err.c_str();
    return false;
  }
  return true;
}

automation::Program compileScript(const char* source) {
  return automation::compile(source, &host);
}

String locoLabel(uint32_t uid) {
  const marklin::LocoState* l = controller.findLoco(uid);
  if (l && l->name[0]) return l->name;
  char ref[24];
  marklin::formatLocoUid(uid, ref, sizeof(ref));
  return ref;
}

String sensorName(uint32_t id) {
  for (const auto& s : config.sensors)
    if (s.id == id) return s.name.c_str();
  return "";
}

void scheduleRestart(uint32_t delayMs) {
  restartAtMs = millis() + delayMs;
  if (restartAtMs == 0) restartAtMs = 1;
}

void appSetup() {
  Serial.begin(115200);
  delay(300);
  eventLog.add(LogKind::Info, "Marklin ESP32: control automatico por CAN (Gleisbox/MS2)");

  if (!LittleFS.begin(true)) eventLog.add(LogKind::Error, "LittleFS no disponible");
  String err;
  if (!loadConfig(&config, &err)) eventLog.addf(LogKind::Info, "config: %s", err.c_str());
  scripts::installDefaults();

  // UID propio derivado de la MAC (único para cada placa).
  const uint64_t mac = ESP.getEfuseMac();
  controller.setUid(0x4D000000u | static_cast<uint32_t>((mac >> 24) & 0xFFFFFF));
  controller.onFrame = logFrame;
  for (const auto& l : config.locos) controller.setLocoName(l.uid, l.name.c_str());

  if (canBus.begin(config.canTxPin, config.canRxPin))
    eventLog.addf(LogKind::Info, "CAN: 250 kbit/s en TX=GPIO%d RX=GPIO%d, hash 0x%04X",
                  config.canTxPin, config.canRxPin, controller.hash());
  else
    eventLog.add(LogKind::Error, "CAN: no se pudo iniciar el controlador TWAI");

  setupLocalSensors();
  if (config.estopPin >= 0) pinMode(config.estopPin, INPUT_PULLUP);

  setupWifi();
  webSetup();
  consoleSetup();

  bootMs = millis();
  controller.ping();  // descubrir equipos (MS2, Gleisbox...)
}

void appLoop() {
  const uint32_t now = millis();
  marklin::CanFrame f;
  for (int i = 0; i < 32 && canBus.receive(&f); i++) controller.handleFrame(f);
  controller.tick(now);
  canBus.poll(now);
  pollLocalSensors(now);
  pollEstop(now);
  bootSequence(now);
  engine.tick();
  webLoop();
  consoleLoop();
  if (restartAtMs && static_cast<int32_t>(now - restartAtMs) >= 0) ESP.restart();
}
