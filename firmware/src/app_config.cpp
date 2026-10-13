#include "app_config.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "automation.h"
#include "marklin_can.h"

static const char* kConfigPath = "/config.json";

static std::string lowerStr(const std::string& s) {
  std::string r = s;
  for (auto& ch : r)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return r;
}

static bool validName(const std::string& s) {
  if (s.empty() || s.size() > 31) return false;
  if (!((s[0] >= 'a' && s[0] <= 'z') || s[0] == '_')) return false;
  for (char ch : s)
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      return false;
  return true;
}

static void readStr(JsonVariantConst v, std::string* out) {
  if (v.is<const char*>()) *out = v.as<const char*>();
}

static void readInt(JsonVariantConst v, int* out) {
  if (v.is<int>()) *out = v.as<int>();
}

static bool validPin(int pin) { return pin >= -1 && pin <= 48; }

bool parseConfig(const String& json, AppConfig* out, String* error) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json);
  if (e) {
    *error = String("JSON no valido: ") + e.c_str();
    return false;
  }
  AppConfig cfg;

  JsonVariantConst wifi = doc["wifi"];
  readStr(wifi["mode"], &cfg.wifiMode);
  readStr(wifi["ssid"], &cfg.staSsid);
  readStr(wifi["password"], &cfg.staPassword);
  readStr(wifi["apSsid"], &cfg.apSsid);
  readStr(wifi["apPassword"], &cfg.apPassword);
  readStr(wifi["hostname"], &cfg.hostname);
  cfg.wifiMode = lowerStr(cfg.wifiMode);
  if (cfg.wifiMode != "ap" && cfg.wifiMode != "sta") {
    *error = "wifi.mode debe ser \"ap\" o \"sta\"";
    return false;
  }
  if (cfg.wifiMode == "sta" && cfg.staSsid.empty()) {
    *error = "wifi.ssid es obligatorio en modo \"sta\"";
    return false;
  }
  if (!cfg.apPassword.empty() && cfg.apPassword.size() < 8) {
    *error = "wifi.apPassword debe tener al menos 8 caracteres (o estar vacia)";
    return false;
  }
  if (cfg.apSsid.empty()) cfg.apSsid = "Marklin-ESP32";
  if (cfg.hostname.empty()) cfg.hostname = "marklin";

  JsonVariantConst can = doc["can"];
  readInt(can["txPin"], &cfg.canTxPin);
  readInt(can["rxPin"], &cfg.canRxPin);
  readStr(can["gleisboxInit"], &cfg.gleisboxInit);
  cfg.gleisboxInit = lowerStr(cfg.gleisboxInit);
  if (cfg.canTxPin < 0 || cfg.canRxPin < 0 || !validPin(cfg.canTxPin) ||
      !validPin(cfg.canRxPin) || cfg.canTxPin == cfg.canRxPin) {
    *error = "can.txPin y can.rxPin deben ser GPIO validos y distintos";
    return false;
  }
  if (cfg.gleisboxInit != "auto" && cfg.gleisboxInit != "always" &&
      cfg.gleisboxInit != "never") {
    *error = "can.gleisboxInit debe ser \"auto\", \"always\" o \"never\"";
    return false;
  }
  if (can["protocols"].is<JsonArrayConst>()) {
    uint8_t mask = 0;
    for (JsonVariantConst p : can["protocols"].as<JsonArrayConst>()) {
      const std::string s = lowerStr(p | "");
      if (s == "mm2" || s == "mm") mask |= marklin::PROTO_MASK_MM2;
      else if (s == "mfx") mask |= marklin::PROTO_MASK_MFX;
      else if (s == "dcc") mask |= marklin::PROTO_MASK_DCC;
      else {
        *error = String("protocolo desconocido en can.protocols: ") + s.c_str();
        return false;
      }
    }
    if (mask) cfg.protocolMask = mask;
  }
  int pulse = cfg.accessoryPulseMs;
  readInt(can["accessoryPulseMs"], &pulse);
  if (pulse < 20 || pulse > 5000) {
    *error = "can.accessoryPulseMs debe estar entre 20 y 5000";
    return false;
  }
  cfg.accessoryPulseMs = static_cast<uint16_t>(pulse);

  readInt(doc["estopPin"], &cfg.estopPin);
  if (!validPin(cfg.estopPin)) {
    *error = "estopPin no valido";
    return false;
  }

  int index = 0;
  for (JsonVariantConst l : doc["locos"].as<JsonArrayConst>()) {
    index++;
    LocoConfig lc;
    lc.name = lowerStr(l["name"] | "");
    const char* ref = l["loco"] | "";
    if (!validName(lc.name)) {
      *error = String("locos[") + index + "]: nombre no valido (letras, numeros, _ y -)";
      return false;
    }
    if (!automation::parseLocoRef(ref, &lc.uid)) {
      *error = String("locos[") + index + "]: direccion no valida '" + ref +
               "' (ej. mm2:24, dcc:3, mfx:5)";
      return false;
    }
    cfg.locos.push_back(lc);
  }

  index = 0;
  for (JsonVariantConst s : doc["sensors"].as<JsonArrayConst>()) {
    index++;
    SensorConfig sc;
    sc.id = s["id"] | 0u;
    sc.name = lowerStr(s["name"] | "");
    sc.pin = s["pin"] | -1;
    sc.activeLow = s["activeLow"] | true;
    sc.debounceMs = s["debounceMs"] | 30;
    if (sc.id == 0) {
      *error = String("sensors[") + index + "]: falta \"id\" (numero > 0)";
      return false;
    }
    if (!sc.name.empty() && !validName(sc.name)) {
      *error = String("sensors[") + index + "]: nombre no valido";
      return false;
    }
    if (!validPin(sc.pin)) {
      *error = String("sensors[") + index + "]: pin no valido";
      return false;
    }
    for (const auto& other : cfg.sensors) {
      if (other.id == sc.id) {
        *error = String("sensors[") + index + "]: id repetido";
        return false;
      }
    }
    cfg.sensors.push_back(sc);
  }

  for (JsonVariantConst a : doc["autostart"].as<JsonArrayConst>()) {
    const char* name = a | "";
    if (*name) cfg.autostart.push_back(name);
  }

  readStr(doc["layout"], &cfg.layout);
  cfg.setupDone = doc["setupDone"] | false;

  *out = cfg;
  return true;
}

String configToJson(const AppConfig& cfg) {
  JsonDocument doc;
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["mode"] = cfg.wifiMode;
  wifi["ssid"] = cfg.staSsid;
  wifi["password"] = cfg.staPassword;
  wifi["apSsid"] = cfg.apSsid;
  wifi["apPassword"] = cfg.apPassword;
  wifi["hostname"] = cfg.hostname;

  JsonObject can = doc["can"].to<JsonObject>();
  can["txPin"] = cfg.canTxPin;
  can["rxPin"] = cfg.canRxPin;
  can["gleisboxInit"] = cfg.gleisboxInit;
  JsonArray protos = can["protocols"].to<JsonArray>();
  if (cfg.protocolMask & marklin::PROTO_MASK_MM2) protos.add("mm2");
  if (cfg.protocolMask & marklin::PROTO_MASK_MFX) protos.add("mfx");
  if (cfg.protocolMask & marklin::PROTO_MASK_DCC) protos.add("dcc");
  can["accessoryPulseMs"] = cfg.accessoryPulseMs;

  doc["estopPin"] = cfg.estopPin;

  JsonArray locos = doc["locos"].to<JsonArray>();
  for (const auto& l : cfg.locos) {
    JsonObject o = locos.add<JsonObject>();
    o["name"] = l.name;
    char ref[24];
    marklin::formatLocoUid(l.uid, ref, sizeof(ref));
    o["loco"] = ref;
  }
  JsonArray sensors = doc["sensors"].to<JsonArray>();
  for (const auto& s : cfg.sensors) {
    JsonObject o = sensors.add<JsonObject>();
    o["id"] = s.id;
    if (!s.name.empty()) o["name"] = s.name;
    o["pin"] = s.pin;
    o["activeLow"] = s.activeLow;
    o["debounceMs"] = s.debounceMs;
  }
  JsonArray autostart = doc["autostart"].to<JsonArray>();
  for (const auto& a : cfg.autostart) autostart.add(a);

  doc["layout"] = cfg.layout;
  doc["setupDone"] = cfg.setupDone;

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool loadConfig(AppConfig* cfg, String* error) {
  if (!LittleFS.exists(kConfigPath)) {
    *error = "no existe /config.json (valores por defecto)";
    return false;
  }
  File f = LittleFS.open(kConfigPath, "r");
  if (!f) {
    *error = "no se pudo abrir /config.json";
    return false;
  }
  String json = f.readString();
  f.close();
  return parseConfig(json, cfg, error);
}

bool saveConfig(const AppConfig& cfg) { return saveConfigJson(configToJson(cfg)); }

bool saveConfigJson(const String& json) {
  const char* tmp = "/config.tmp";
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  const size_t written = f.print(json);
  f.close();
  if (written != json.length()) return false;
  LittleFS.remove(kConfigPath);
  return LittleFS.rename(tmp, kConfigPath);
}
