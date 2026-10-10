#include "web_ui.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#include "app.h"
#include "event_log.h"
#include "script_store.h"
#include "web_page.h"

namespace {

WebServer server(80);

void sendJson(const JsonDocument& doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

void sendOk(const String& msg = "ok") {
  JsonDocument d;
  d["ok"] = true;
  d["msg"] = msg;
  sendJson(d);
}

void sendError(const String& msg, int code = 400) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  sendJson(d, code);
}

String body() { return server.arg("plain"); }

bool argLoco(uint32_t* uid) {
  const String ref = server.arg("loco");
  return automation::parseLocoRef(ref.c_str(), uid);
}

String hex32(uint32_t v) {
  char buf[12];
  snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(v));
  return buf;
}

const char* powerName(marklin::PowerState p) {
  switch (p) {
    case marklin::PowerState::On: return "on";
    case marklin::PowerState::Off: return "off";
    default: return "unknown";
  }
}

// ---- Estado ------------------------------------------------------------------------

void handleState() {
  JsonDocument doc;
  doc["power"] = powerName(controller.power());
  doc["overload"] = controller.overload();
  doc["uptime"] = millis() / 1000;
  doc["logSeq"] = eventLog.lastSeq();
  doc["ms2"] = controller.ms2Present();

  JsonObject can = doc["can"].to<JsonObject>();
  can["state"] = canBus.stateName();
  can["rx"] = controller.framesRx();
  can["tx"] = controller.framesTx();
  can["txFail"] = controller.txErrors();
  can["tec"] = canBus.txErrorCounter();
  can["rec"] = canBus.rxErrorCounter();
  can["busErrors"] = canBus.busErrors();
  char hash[8];
  snprintf(hash, sizeof(hash), "0x%04X", controller.hash());
  can["hash"] = hash;

  JsonArray locos = doc["locos"].to<JsonArray>();
  for (int i = 0; i < controller.locoCount(); i++) {
    const marklin::LocoState& l = controller.loco(i);
    JsonObject o = locos.add<JsonObject>();
    char ref[24];
    marklin::formatLocoUid(l.uid, ref, sizeof(ref));
    o["ref"] = ref;
    o["name"] = l.name;
    o["speed"] = l.speed;
    o["dir"] = l.direction;
    o["fn"] = l.functions;
  }

  JsonArray sensors = doc["sensors"].to<JsonArray>();
  for (int i = 0; i < controller.sensorCount(); i++) {
    const marklin::SensorState& s = controller.sensor(i);
    JsonObject o = sensors.add<JsonObject>();
    o["id"] = s.id;
    o["name"] = sensorName(s.id);
    o["state"] = s.state;
  }

  JsonArray accs = doc["accessories"].to<JsonArray>();
  for (int i = 0; i < controller.accessoryCount(); i++) {
    const marklin::AccessoryState& a = controller.accessory(i);
    marklin::Protocol p;
    uint32_t addr;
    if (!marklin::decodeAccessoryUid(a.uid, &p, &addr)) continue;
    JsonObject o = accs.add<JsonObject>();
    char ref[16];
    snprintf(ref, sizeof(ref), "%s:%lu", p == marklin::Protocol::DCC ? "dcc" : "mm",
             static_cast<unsigned long>(addr));
    o["ref"] = ref;
    o["pos"] = a.position;
  }

  JsonArray runners = doc["runners"].to<JsonArray>();
  const uint32_t now = millis();
  for (int i = 0; i < automation::Engine::kMaxRunners; i++) {
    const automation::Runner& r = engine.runner(i);
    if (r.state() == automation::RunState::Idle) continue;
    JsonObject o = runners.add<JsonObject>();
    o["slot"] = i;
    o["name"] = r.name();
    o["state"] = automation::runStateName(r.state());
    o["active"] = r.active();
    o["line"] = r.line();
    o["activity"] = r.activity(now);
  }

  JsonArray devices = doc["devices"].to<JsonArray>();
  for (int i = 0; i < controller.deviceCount(); i++) {
    const marklin::DeviceInfo& d = controller.device(i);
    JsonObject o = devices.add<JsonObject>();
    o["type"] = marklin::deviceTypeName(d.type, d.uid);
    o["uid"] = hex32(d.uid);
    char ver[12];
    snprintf(ver, sizeof(ver), "%u.%u", d.versionHigh, d.versionLow);
    o["version"] = ver;
  }

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  const bool sta = WiFi.getMode() == WIFI_STA;
  wifi["mode"] = sta ? "sta" : "ap";
  wifi["ip"] = sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  if (sta) wifi["rssi"] = WiFi.RSSI();
  doc["heap"] = ESP.getFreeHeap();
  sendJson(doc);
}

void handleLog() {
  const uint32_t since = static_cast<uint32_t>(server.arg("since").toInt());
  JsonDocument doc;
  doc["seq"] = eventLog.lastSeq();
  JsonArray entries = doc["entries"].to<JsonArray>();
  eventLog.forEachSince(since, [&](const LogEntry& e) {
    JsonObject o = entries.add<JsonObject>();
    o["s"] = e.seq;
    o["t"] = e.ms;
    o["k"] = EventLog::kindName(e.kind);
    o["m"] = e.text;
  });
  sendJson(doc);
}

// ---- Mando -------------------------------------------------------------------------

void handlePower() {
  const bool on = server.arg("on") == "1";
  controller.setTrackPower(on);
  sendOk(on ? "corriente ON" : "corriente OFF");
}

void handleEmergency() {
  emergencyStop("boton de la web");
  sendOk("parada de emergencia");
}

void handleHalt() {
  const int n = controller.stopAllLocos();
  sendOk(String(n) + " locos paradas");
}

void handleLoco() {
  uint32_t uid;
  if (!argLoco(&uid)) return sendError("loco no valida (ej. mm2:24, dcc:3, mfx:5)");
  if (server.hasArg("speed")) {
    long s = server.arg("speed").toInt();
    if (s < 0) s = 0;
    if (s > marklin::kMaxSpeed) s = marklin::kMaxSpeed;
    controller.setSpeed(uid, static_cast<uint16_t>(s));
  }
  if (server.hasArg("dir")) {
    const String d = server.arg("dir");
    marklin::Direction dir = d == "fwd"   ? marklin::DIR_FORWARD
                             : d == "rev" ? marklin::DIR_REVERSE
                                          : marklin::DIR_TOGGLE;
    controller.setDirection(uid, dir);
  }
  if (server.hasArg("fn")) {
    const long fn = server.arg("fn").toInt();
    if (fn < 0 || fn > 31) return sendError("funcion fuera de rango (0-31)");
    controller.setFunction(uid, static_cast<uint8_t>(fn), server.arg("on") == "1");
  }
  if (server.hasArg("estop")) controller.emergencyStop(uid);
  if (server.hasArg("add")) {
    if (!controller.touchLoco(uid)) return sendError("tabla de locos llena");
    controller.queryLoco(uid);
  }
  sendOk();
}

void handleAccessory() {
  uint32_t uid;
  if (!automation::parseAccessoryRef(server.arg("addr").c_str(), &uid))
    return sendError("direccion de accesorio no valida (ej. 5, mm:5, dcc:12)");
  const long pos = server.arg("pos").toInt();
  if (pos < 0 || pos > 3) return sendError("posicion no valida (0-3)");
  controller.switchAccessory(uid, static_cast<uint8_t>(pos), config.accessoryPulseMs);
  sendOk();
}

void handleSensor() {
  const long id = server.arg("id").toInt();
  if (id <= 0) return sendError("id de sensor no valido");
  controller.setSensor(static_cast<uint32_t>(id), server.arg("state") == "1");
  eventLog.addf(LogKind::Info, "sensor %ld simulado = %s", id,
                server.arg("state") == "1" ? "on" : "off");
  sendOk();
}

void handlePing() {
  controller.ping();
  sendOk("ping enviado");
}

// ---- Scripts -----------------------------------------------------------------------

void handleScriptList() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const String& n : scripts::list()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = n;
    o["running"] = engine.isRunning(n.c_str());
  }
  sendJson(doc);
}

void handleScriptGet() {
  String src;
  if (!scripts::read(server.arg("name"), &src)) return sendError("no existe", 404);
  server.send(200, "text/plain; charset=utf-8", src);
}

void fillCompileResult(JsonDocument& doc, const String& src) {
  const automation::Program p = compileScript(src.c_str());
  doc["valid"] = p.ok();
  if (!p.ok()) {
    doc["line"] = p.errorLine;
    doc["error"] = p.error;
  }
}

void handleScriptSave() {
  const String name = server.arg("name");
  if (!scripts::validName(name))
    return sendError("nombre no valido: usa minusculas, numeros, _ o - (max. 31)");
  const String src = body();
  if (src.length() > scripts::kMaxScriptSize) return sendError("script demasiado grande");
  if (!scripts::write(name, src)) return sendError("no se pudo guardar", 500);
  JsonDocument doc;
  doc["ok"] = true;
  fillCompileResult(doc, src);
  sendJson(doc);
}

void handleScriptDelete() {
  const String name = server.arg("name");
  if (engine.isRunning(name.c_str())) return sendError("detén el script antes de borrarlo");
  if (!scripts::remove(name)) return sendError("no se pudo borrar");
  sendOk("borrado");
}

void handleScriptCheck() {
  JsonDocument doc;
  doc["ok"] = true;
  fillCompileResult(doc, body());
  sendJson(doc);
}

void handleRun() {
  String err;
  if (!startScript(server.arg("name"), &err)) return sendError(err);
  sendOk("iniciado");
}

void handleScriptStop() {
  if (server.hasArg("all")) {
    engine.stopAll();
    return sendOk("todos detenidos");
  }
  bool ok = server.hasArg("slot") ? engine.stop(server.arg("slot").toInt())
                                  : engine.stopByName(server.arg("name").c_str());
  if (!ok) return sendError("no estaba en marcha");
  sendOk("detenido");
}

void handleExec() {
  const String line = body();
  const std::string err = engine.exec(line.c_str());
  if (!err.empty()) return sendError(err.c_str());
  sendOk();
}

// ---- Configuración -----------------------------------------------------------------

void handleConfigGet() { server.send(200, "application/json", configToJson(config)); }

void handleConfigSave() {
  const String json = body();
  AppConfig parsed;
  String err;
  if (!parseConfig(json, &parsed, &err)) return sendError(err);
  if (!saveConfigJson(configToJson(parsed))) return sendError("no se pudo guardar", 500);
  eventLog.add(LogKind::Info, "configuracion guardada, reiniciando...");
  sendOk("guardado; reiniciando");
  scheduleRestart(800);
}

void handleRestart() {
  sendOk("reiniciando");
  scheduleRestart(500);
}

}  // namespace

void webSetup() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", kIndexHtml); });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/power", HTTP_POST, handlePower);
  server.on("/api/emergency", HTTP_POST, handleEmergency);
  server.on("/api/halt", HTTP_POST, handleHalt);
  server.on("/api/loco", HTTP_POST, handleLoco);
  server.on("/api/accessory", HTTP_POST, handleAccessory);
  server.on("/api/sensor", HTTP_POST, handleSensor);
  server.on("/api/ping", HTTP_POST, handlePing);
  server.on("/api/scripts", HTTP_GET, handleScriptList);
  server.on("/api/script", HTTP_GET, handleScriptGet);
  server.on("/api/script", HTTP_POST, handleScriptSave);
  server.on("/api/script/delete", HTTP_POST, handleScriptDelete);
  server.on("/api/check", HTTP_POST, handleScriptCheck);
  server.on("/api/run", HTTP_POST, handleRun);
  server.on("/api/scriptstop", HTTP_POST, handleScriptStop);
  server.on("/api/exec", HTTP_POST, handleExec);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigSave);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.onNotFound([] { server.send(404, "text/plain", "no encontrado"); });
  server.begin();
}

void webLoop() { server.handleClient(); }
