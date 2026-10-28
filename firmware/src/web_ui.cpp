#include "web_ui.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

#include <algorithm>

#include "app.h"
#include "cs2_server.h"
#include "event_log.h"
#include "inventory.h"
#include "layout_store.h"
#include "script_store.h"
#include "version.h"
#include "web_assets.h"

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
  doc["version"] = FW_VERSION;
  doc["setupDone"] = config.setupDone;
  doc["layout"] = config.layout;
  doc["power"] = powerName(controller.power());
  doc["overload"] = controller.overload();
  doc["uptime"] = millis() / 1000;
  doc["logSeq"] = eventLog.lastSeq();
  doc["ms2"] = controller.ms2Present();
  doc["inv"] = inventory::seq();
  JsonObject imp = doc["imp"].to<JsonObject>();
  imp["busy"] = inventory::importBusy();
  imp["msg"] = inventory::importStatus();
  imp["auto"] = config.autoImport;

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
    for (const auto& c : config.locos)
      if (c.uid == l.uid) {
        o["reg"] = true;
        if (!c.label.empty()) o["label"] = c.label;
      }
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
  const bool sta = WiFi.status() == WL_CONNECTED;
  wifi["mode"] = config.wifiMode;
  wifi["connected"] = sta;
  wifi["ssid"] = config.staSsid;
  wifi["apSsid"] = config.apSsid;
  wifi["apIp"] = WiFi.softAPIP().toString();
  wifi["ip"] = sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  wifi["hostname"] = config.hostname;

  JsonObject lan = doc["lan"].to<JsonObject>();
  lan["tcp"] = cs2server::tcpClients();
  lan["udp"] = cs2server::udpPeers();
  lan["frames"] = cs2server::framesFromApps();
  lan["files"] = cs2server::configRequests();
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
  sendOk(on ? "track power ON" : "track power OFF");
}

void handleEmergency() {
  emergencyStop("web button");
  sendOk("emergency stop");
}

void handleHalt() {
  const int n = controller.stopAllLocos();
  sendOk(String(n) + " locos stopped");
}

void handleLoco() {
  uint32_t uid;
  if (!argLoco(&uid)) return sendError("invalid loco (e.g. mm2:24, dcc:3, mfx:5)");
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
    if (fn < 0 || fn > 31) return sendError("function out of range (0-31)");
    controller.setFunction(uid, static_cast<uint8_t>(fn), server.arg("on") == "1");
  }
  if (server.hasArg("estop")) controller.emergencyStop(uid);
  if (server.hasArg("add")) {
    if (!controller.touchLoco(uid)) return sendError("loco table full");
    controller.queryLoco(uid);
  }
  sendOk();
}

void handleAccessory() {
  uint32_t uid;
  if (!automation::parseAccessoryRef(server.arg("addr").c_str(), &uid))
    return sendError("invalid accessory address (e.g. 5, mm:5, dcc:12)");
  const long pos = server.arg("pos").toInt();
  if (pos < 0 || pos > 3) return sendError("invalid position (0-3)");
  controller.switchAccessory(uid, static_cast<uint8_t>(pos), config.accessoryPulseMs);
  sendOk();
}

void handleSensor() {
  const long id = server.arg("id").toInt();
  if (id <= 0) return sendError("invalid sensor id");
  controller.setSensor(static_cast<uint32_t>(id), server.arg("state") == "1");
  cs2server::notifySensor(static_cast<uint32_t>(id), server.arg("state") == "1");
  eventLog.addf(LogKind::Info, "sensor %ld simulated = %s", id,
                server.arg("state") == "1" ? "on" : "off");
  sendOk();
}

void handlePing() {
  controller.ping();
  sendOk("ping sent");
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
  if (!scripts::read(server.arg("name"), &src)) return sendError("does not exist", 404);
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
    return sendError("invalid name: use lowercase letters, digits, _ or - (max. 31)");
  const String src = body();
  if (src.length() > scripts::kMaxScriptSize) return sendError("script too large");
  if (!scripts::write(name, src)) return sendError("could not save", 500);
  JsonDocument doc;
  doc["ok"] = true;
  fillCompileResult(doc, src);
  sendJson(doc);
}

void handleScriptDelete() {
  const String name = server.arg("name");
  if (engine.isRunning(name.c_str())) return sendError("stop the script before deleting it");
  if (!scripts::remove(name)) return sendError("could not delete");
  sendOk("deleted");
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
  sendOk("started");
}

void handleScriptStop() {
  if (server.hasArg("all")) {
    engine.stopAll();
    return sendOk("all stopped");
  }
  bool ok = server.hasArg("slot") ? engine.stop(server.arg("slot").toInt())
                                  : engine.stopByName(server.arg("name").c_str());
  if (!ok) return sendError("it was not running");
  sendOk("stopped");
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
  if (!saveConfigJson(configToJson(parsed))) return sendError("could not save", 500);
  eventLog.add(LogKind::Info, "configuration saved, restarting...");
  sendOk("saved; restarting");
  scheduleRestart(800);
}

void handleRestart() {
  sendOk("restarting");
  scheduleRestart(500);
}

// Registro de locomotoras (se guarda en la configuración):
//   loco=mm2:24 name=cocodrilo label="Cocodrilo SBB"   alta o cambio
//   old=mm2:23                                          si cambia la dirección
//   delete=1                                            baja
void handleLocoReg() {
  uint32_t uid;
  if (!argLoco(&uid)) return sendError("invalid loco address (e.g. mm2:24, dcc:3, mfx:5)");
  auto find = [](uint32_t u) {
    return std::find_if(config.locos.begin(), config.locos.end(),
                        [u](const LocoConfig& l) { return l.uid == u; });
  };
  if (server.hasArg("delete")) {
    auto it = find(uid);
    if (it == config.locos.end()) return sendError("that loco is not registered");
    config.locos.erase(it);
    if (!saveConfig(config)) return sendError("could not save", 500);
    controller.setLocoName(uid, "");
    inventory::removePhoto(uid);
    inventory::touch();
    return sendOk("loco removed from the inventory");
  }
  String name = server.arg("name");
  name.trim();
  name.toLowerCase();
  const std::string n = name.c_str();
  if (!layouts::validName(name) || !(n[0] >= 'a' && n[0] <= 'z'))
    return sendError("invalid script name: start with a letter; only lowercase letters, digits, _ and -");
  String label = server.arg("label");
  label.trim();
  if (label.length() > 60) label = label.substring(0, 60);

  uint32_t oldUid = uid;
  if (server.hasArg("old") && server.arg("old").length()) {
    if (!automation::parseLocoRef(server.arg("old").c_str(), &oldUid))
      return sendError("invalid previous address");
  }
  for (const auto& l : config.locos) {
    if (l.uid == oldUid) continue;
    if (l.name == n) return sendError("another loco already has that name");
    if (l.uid == uid) return sendError("another loco is already registered with that address");
  }
  auto it = find(oldUid);
  if (it == config.locos.end()) {
    config.locos.push_back(LocoConfig());
    it = config.locos.end() - 1;
  }
  it->uid = uid;
  it->name = n;
  it->label = label.c_str();
  if (server.hasArg("fn")) {
    // Tipos de función separados por comas: "1,5,138" (0 = sin función).
    it->fn.clear();
    const String list = server.arg("fn");
    int start = 0;
    while (start < static_cast<int>(list.length()) && it->fn.size() < 32) {
      int end = list.indexOf(',', start);
      if (end < 0) end = list.length();
      it->fn.push_back(static_cast<uint8_t>(list.substring(start, end).toInt() & 0xFF));
      start = end + 1;
    }
    while (!it->fn.empty() && it->fn.back() == 0) it->fn.pop_back();
  }
  if (!saveConfig(config)) return sendError("could not save", 500);
  inventory::renamePhoto(oldUid, uid);
  inventory::touch();
  if (oldUid != uid) controller.setLocoName(oldUid, "");
  controller.setLocoName(uid, n.c_str());
  controller.queryLoco(uid);
  sendOk(String("registered: ") + (label.length() ? label : name));
}

// Asistente de primera configuración.
//   {"locos":[{"name":"x","loco":"mm2:24"}], "layout":"station",
//    "wifi":{"mode":"sta","ssid":"...","password":"..."}}
void handleSetup() {
  JsonDocument in;
  if (deserializeJson(in, body())) return sendError("invalid data");
  JsonDocument cfg;
  deserializeJson(cfg, configToJson(config));
  if (in["locos"].is<JsonArrayConst>()) {
    cfg["locos"] = in["locos"];
    // Conserva las funciones y datos mfx de las locos que ya estaban.
    for (JsonObject l : cfg["locos"].as<JsonArray>()) {
      uint32_t uid;
      if (!automation::parseLocoRef(l["loco"] | "", &uid)) continue;
      for (const auto& c : config.locos) {
        if (c.uid != uid) continue;
        if (!l["label"].is<const char*>() && !c.label.empty()) l["label"] = c.label;
        if (!c.fn.empty()) {
          JsonArray fn = l["fn"].to<JsonArray>();
          for (uint8_t t : c.fn) fn.add(t);
        }
        if (c.mfxUid) l["mfxuid"] = c.mfxUid;
      }
    }
  }
  if (in["layout"].is<const char*>()) cfg["layout"] = in["layout"];
  JsonVariantConst w = in["wifi"];
  if (w["mode"].is<const char*>()) {
    cfg["wifi"]["mode"] = w["mode"];
    cfg["wifi"]["ssid"] = w["ssid"] | "";
    cfg["wifi"]["password"] = w["password"] | "";
  }
  cfg["setupDone"] = true;
  String json;
  serializeJsonPretty(cfg, json);
  AppConfig parsed;
  String err;
  if (!parseConfig(json, &parsed, &err)) return sendError(err);
  if (!parsed.layout.empty() && !layouts::exists(parsed.layout.c_str()))
    return sendError("the chosen layout does not exist");
  const bool wifiChanged = parsed.wifiMode != config.wifiMode || parsed.staSsid != config.staSsid ||
                           parsed.staPassword != config.staPassword;
  if (!saveConfigJson(json)) return sendError("could not save", 500);
  config = parsed;
  for (const auto& l : config.locos) controller.setLocoName(l.uid, l.name.c_str());
  inventory::touch();
  activateLayout(config.layout);
  JsonDocument d;
  d["ok"] = true;
  d["restart"] = wifiChanged;
  d["msg"] = "configuration saved";
  sendJson(d);
  eventLog.add(LogKind::Info, "setup wizard completed");
  if (wifiChanged) scheduleRestart(1500);
}

// ---- Maquetas ------------------------------------------------------------------------

void handleLayoutList() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const String& n : layouts::list()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = n;
    o["title"] = layouts::title(n);
    o["active"] = config.layout == n.c_str();
    o["gbs"] = layouts::hasTrackPlan(n);
  }
  sendJson(doc);
}

void handleLayoutGet() {
  String json;
  if (!layouts::read(server.arg("name"), &json)) return sendError("does not exist", 404);
  server.send(200, "application/json", json);
}

void handleLayoutSave() {
  const String name = server.arg("name");
  String err;
  if (!layouts::write(name, body(), &err)) return sendError(err);
  if (config.layout == name.c_str()) activateLayout(config.layout);
  sendOk("layout saved");
}

void handleLayoutTrackPlan() {
  if (!layouts::writeTrackPlan(server.arg("name"), body())) return sendError("could not save the track plan");
  sendOk("track plan saved");
}

void handleLayoutDelete() {
  const String name = server.arg("name");
  if (!layouts::remove(name)) return sendError("could not delete");
  if (config.layout == name.c_str()) {
    config.layout.clear();
    saveConfig(config);
    activateLayout("");
  }
  sendOk("layout deleted");
}

void handleLayoutActivate() {
  const String name = server.arg("name");
  if (name.length() && !layouts::exists(name)) return sendError("that layout does not exist");
  config.layout = name.c_str();
  if (!saveConfig(config)) return sendError("could not save", 500);
  activateLayout(config.layout);
  sendOk(name.length() ? String("active layout: ") + name : String("no active layout"));
}

// ---- Interfaz y portal cautivo ---------------------------------------------------------

void sendGz(const char* type, const uint8_t* data, size_t len) {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, type, reinterpret_cast<const char*>(data), len);
}

void redirectToApp() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

// ---- Programación de decodificadores --------------------------------------------------

// POST /api/cv?loco=dcc:3&cv=29&index=0&op=read|write&value=6&main=1
void handleCvOp() {
  uint32_t uid;
  if (!argLoco(&uid)) return sendError("invalid loco address");
  const long cv = server.arg("cv").toInt(), index = server.arg("index").toInt();
  if (cv < 1 || cv > 1023) return sendError("invalid CV (1-1023)");
  if (index < 0 || index > 63) return sendError("invalid index (0-63)");
  bool sent;
  if (server.arg("op") == "write") {
    const long v = server.arg("value").toInt();
    if (!server.hasArg("value") || v < 0 || v > 255) return sendError("invalid value (0-255)");
    sent = controller.writeConfig(uid, cv, index, v, server.arg("main") == "1");
  } else {
    sent = controller.readConfig(uid, cv, index);
  }
  if (!sent) return sendError("could not send to the CAN bus");
  JsonDocument d;
  d["ok"] = true;
  d["seq"] = controller.configResult().seq;
  sendJson(d);
}

void handleCvResult() {
  const marklin::ConfigResult& r = controller.configResult();
  JsonDocument d;
  d["seq"] = r.seq;
  char ref[24];
  marklin::formatLocoUid(r.uid, ref, sizeof(ref));
  d["loco"] = ref;
  d["cv"] = r.cv;
  d["index"] = r.index;
  d["write"] = r.write;
  d["ok"] = r.ok;
  d["verified"] = r.verified;
  d["value"] = r.value;
  sendJson(d);
}

// ---- Inventario -----------------------------------------------------------------------

void handleInventory() {
  JsonDocument doc;
  doc["seq"] = inventory::seq();
  doc["auto"] = config.autoImport;
  JsonArray arr = doc["locos"].to<JsonArray>();
  for (const auto& l : config.locos) {
    JsonObject o = arr.add<JsonObject>();
    char ref[24];
    marklin::formatLocoUid(l.uid, ref, sizeof(ref));
    o["ref"] = ref;
    o["name"] = l.name;
    o["label"] = l.label;
    JsonArray fn = o["fn"].to<JsonArray>();
    for (uint8_t t : l.fn) fn.add(t);
    o["photo"] = inventory::hasPhoto(l.uid);
    if (l.mfxUid) o["mfxuid"] = hex32(l.mfxUid);
  }
  sendJson(doc);
}

void handleMs2Import() {
  String err;
  if (!inventory::startImport(&err)) return sendError(err);
  sendOk("reading the locos from the MS2...");
}

void handleAutoImport() {
  config.autoImport = server.arg("on") == "1";
  if (!saveConfig(config)) return sendError("could not save", 500);
  sendOk(config.autoImport ? "automatic reading enabled" : "automatic reading disabled");
}

// Fotos: la web las reduce (JPEG de ~30 KB) y las sube como formulario.
File photoFile;
String photoError;
uint32_t photoUid = 0;
constexpr size_t kMaxPhoto = 300 * 1024;

void handlePhotoUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    photoError = "";
    if (!argLoco(&photoUid)) {
      photoError = "invalid loco address";
      return;
    }
    photoFile = LittleFS.open("/photos/subida.tmp", "w");
    if (!photoFile) photoError = "could not create the file";
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (!photoFile) return;
    if (up.totalSize > kMaxPhoto) {
      photoError = "the photo is too large";
      photoFile.close();
      LittleFS.remove("/photos/subida.tmp");
      return;
    }
    if (photoFile.write(up.buf, up.currentSize) != up.currentSize) photoError = "memory full";
  } else if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (photoFile) photoFile.close();
    if (up.status == UPLOAD_FILE_ABORTED) photoError = "upload interrupted";
  }
}

void handlePhotoDone() {
  if (photoError.length() || !LittleFS.exists("/photos/subida.tmp")) {
    LittleFS.remove("/photos/subida.tmp");
    return sendError(photoError.length() ? photoError : "no photo received");
  }
  const String path = inventory::photoPath(photoUid);
  LittleFS.remove(path);
  if (!LittleFS.rename("/photos/subida.tmp", path)) return sendError("could not save", 500);
  inventory::touch();
  sendOk("photo saved");
}

void handlePhotoGet() {
  uint32_t uid;
  if (!argLoco(&uid)) return sendError("invalid loco address");
  File f = LittleFS.open(inventory::photoPath(uid), "r");
  if (!f) return sendError("no photo", 404);
  server.sendHeader("Cache-Control", "max-age=31536000");
  server.streamFile(f, "image/jpeg");
  f.close();
}

void handlePhotoDelete() {
  uint32_t uid;
  if (!argLoco(&uid)) return sendError("invalid loco address");
  inventory::removePhoto(uid);
  sendOk("photo removed");
}

// Ficheros de configuración de la central, como en la CS2:
// /config/lokomotive.cs2, magnetartikel.cs2, gleisbild.cs2, gleisbilder/<página>.cs2
// y geraet.vrs (las apps lo piden para saber si hay una CS2 en esa IP).
bool serveCs2Config(const String& uri) {
  if (!uri.startsWith("/config/")) return false;
  const String file = uri.substring(8);
  std::string request;
  if (file == "lokomotive.cs2") request = "loks";
  else if (file == "magnetartikel.cs2") request = "mags";
  else if (file == "gleisbild.cs2") request = "gbs";
  else if (file == "fahrstrassen.cs2") request = "fs";
  else if (file == "geraet.vrs") request = "geraet";
  else if (file == "prefs.cs2") request = "prefs";
  else if (file == "lokomotive.sr2") request = "lokstat";
  else if (file == "magnetartikel.sr2") request = "magstat";
  else if (file == "gleisbild.sr2") request = "gbsstat";
  else if (file == "fahrstrassen.sr2") request = "fsstat";
  else if (file.startsWith("gleisbilder/") && file.endsWith(".cs2")) request = "gbs-0";
  else return false;
  std::string content;
  if (!cs2server::configFile(request, &content)) return false;
  server.send(200, "text/plain; charset=utf-8", content.c_str());
  eventLog.addf(LogKind::Info, "HTTP: %s (%u bytes) to %s", uri.c_str(), static_cast<unsigned>(content.size()),
                server.client().remoteIP().toString().c_str());
  return true;
}

void handleNotFound() {
  if (server.method() == HTTP_GET && serveCs2Config(server.uri())) return;
  // En el punto de acceso, cualquier dominio desconocido lleva a la app.
  const String host = server.hostHeader();
  if (isCaptivePortal() && host != WiFi.softAPIP().toString() && host != WiFi.localIP().toString() &&
      host != String(config.hostname.c_str()) + ".local") {
    return redirectToApp();
  }
  // Para diagnosticar apps (p.ej. RailControl Pro) que piden algo que no existe.
  if (server.uri() != "/favicon.ico")
    eventLog.addf(LogKind::Info, "HTTP: unknown request %s %s from %s",
                  server.method() == HTTP_GET ? "GET" : "POST/other", server.uri().c_str(),
                  server.client().remoteIP().toString().c_str());
  server.send(404, "text/plain", "not found");
}

}  // namespace

void webSetup() {
  server.on("/", HTTP_GET, [] { sendGz("text/html; charset=utf-8", kIndexHtmlGz, sizeof(kIndexHtmlGz)); });
  server.on("/manifest.json", HTTP_GET, [] { server.send_P(200, "application/manifest+json", kManifestJson); });
  server.on("/icon.svg", HTTP_GET, [] { server.send_P(200, "image/svg+xml", kIconSvg); });
  server.on("/icon-192.png", HTTP_GET, [] {
    server.send_P(200, "image/png", reinterpret_cast<const char*>(kIconPng), sizeof(kIconPng));
  });
  // Detección de portal cautivo de Android, iOS/macOS y Windows.
  for (const char* url : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
                          "/connecttest.txt", "/ncsi.txt", "/redirect", "/canonical.html", "/success.txt"})
    server.on(url, HTTP_GET, redirectToApp);
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
  server.on("/api/locoreg", HTTP_POST, handleLocoReg);
  server.on("/api/inventory", HTTP_GET, handleInventory);
  server.on("/api/cv", HTTP_POST, handleCvOp);
  server.on("/api/cv", HTTP_GET, handleCvResult);
  server.on("/api/ms2import", HTTP_POST, handleMs2Import);
  server.on("/api/autoimport", HTTP_POST, handleAutoImport);
  server.on("/api/photo", HTTP_GET, handlePhotoGet);
  server.on("/api/photo", HTTP_POST, handlePhotoDone, handlePhotoUpload);
  server.on("/api/photo/delete", HTTP_POST, handlePhotoDelete);
  server.on("/api/setup", HTTP_POST, handleSetup);
  server.on("/api/layouts", HTTP_GET, handleLayoutList);
  server.on("/api/layout", HTTP_GET, handleLayoutGet);
  server.on("/api/layout", HTTP_POST, handleLayoutSave);
  server.on("/api/layout/delete", HTTP_POST, handleLayoutDelete);
  server.on("/api/layout/gbs", HTTP_POST, handleLayoutTrackPlan);
  server.on("/api/layout/activate", HTTP_POST, handleLayoutActivate);
  server.onNotFound(handleNotFound);
  server.begin();
}

void webLoop() { server.handleClient(); }
