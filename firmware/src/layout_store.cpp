#include "layout_store.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include <algorithm>

#include "automation.h"
#include "default_layouts.h"
#include "event_log.h"

namespace layouts {

static const char* kDir = "/layouts";
static const char* kDefaultsMarker = "/layouts/.ejemplos";

static std::vector<NamedAccessory> activeAccessories;
static std::vector<NamedSensor> activeSensors;
static std::vector<LayoutTurnout> activeTurnouts;
static std::string activeName;

static String pathFor(const String& name) { return String(kDir) + "/" + name + ".json"; }
static String planFor(const String& name) { return String(kDir) + "/" + name + ".gbs"; }

bool validName(const String& name) {
  if (name.length() == 0 || name.length() > 31) return false;
  for (size_t i = 0; i < name.length(); i++) {
    const char ch = name[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      return false;
  }
  return true;
}

std::vector<String> list() {
  std::vector<String> out;
  File dir = LittleFS.open(kDir);
  if (!dir || !dir.isDirectory()) return out;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String n = f.name();
    const int slash = n.lastIndexOf('/');
    if (slash >= 0) n = n.substring(slash + 1);
    if (!f.isDirectory() && n.endsWith(".json")) out.push_back(n.substring(0, n.length() - 5));
  }
  std::sort(out.begin(), out.end(), [](const String& a, const String& b) { return a < b; });
  return out;
}

bool exists(const String& name) { return validName(name) && LittleFS.exists(pathFor(name)); }

bool read(const String& name, String* out) {
  if (!validName(name)) return false;
  File f = LittleFS.open(pathFor(name), "r");
  if (!f) return false;
  *out = f.readString();
  f.close();
  return true;
}

bool write(const String& name, const String& json, String* error) {
  if (!validName(name)) {
    *error = "invalid name: use lowercase letters, digits, _ or - (max. 31)";
    return false;
  }
  if (json.length() > kMaxLayoutSize) {
    *error = "layout too large (max. 48 KB)";
    return false;
  }
  // Validar sin cargar todo en memoria: solo las referencias de las piezas.
  JsonDocument filter;
  filter["pieces"][0]["t"] = true;
  JsonDocument doc;
  const DeserializationError e = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (e) {
    *error = String("invalid JSON: ") + e.c_str();
    return false;
  }
  if (!doc["pieces"].is<JsonArray>() || doc["pieces"].size() == 0) {
    *error = "not a C-Track Planner layout (the pieces are missing)";
    return false;
  }
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  LittleFS.remove(planFor(name));
  File f = LittleFS.open(pathFor(name), "w");
  if (!f) {
    *error = "could not save";
    return false;
  }
  const size_t n = f.print(json);
  f.close();
  if (n != json.length()) {
    *error = "not enough free memory";
    LittleFS.remove(pathFor(name));
    return false;
  }
  return true;
}

bool remove(const String& name) {
  if (!validName(name)) return false;
  LittleFS.remove(planFor(name));
  return LittleFS.remove(pathFor(name));
}

bool writeTrackPlan(const String& name, const String& text) {
  if (!exists(name) || text.length() > kMaxTrackPlanSize || !text.startsWith("[gleisbildseite]")) return false;
  File f = LittleFS.open(planFor(name), "w");
  if (!f) return false;
  const bool ok = f.print(text) == text.length();
  f.close();
  if (!ok) LittleFS.remove(planFor(name));
  return ok;
}

bool readTrackPlan(const String& name, std::string* out) {
  if (!validName(name)) return false;
  File f = LittleFS.open(planFor(name), "r");
  if (!f) return false;
  out->resize(f.size());
  const size_t n = f.read(reinterpret_cast<uint8_t*>(&(*out)[0]), out->size());
  f.close();
  out->resize(n);
  return n > 0;
}

bool hasTrackPlan(const String& name) { return validName(name) && LittleFS.exists(planFor(name)); }

String title(const String& name) {
  File f = LittleFS.open(pathFor(name), "r");
  if (!f) return name;
  JsonDocument filter;
  filter["name"] = true;
  JsonDocument doc;
  deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  const char* t = doc["name"] | "";
  return *t ? String(t) : name;
}

void installDefaults() {
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  if (LittleFS.exists(kDefaultsMarker)) return;
  for (int i = 0; i < kDefaultLayoutCount; i++) {
    String err;
    const String name = kDefaultLayouts[i].name;
    if (!exists(name)) write(name, kDefaultLayouts[i].json, &err);
  }
  File m = LittleFS.open(kDefaultsMarker, "w");
  if (m) {
    m.print("1");
    m.close();
  }
  eventLog.addf(LogKind::Info, "installed %d example layouts", kDefaultLayoutCount);
}

static std::string lowerStr(const char* s) {
  std::string r = s ? s : "";
  for (auto& ch : r)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return r;
}

void loadActive(const std::string& name) {
  activeAccessories.clear();
  activeSensors.clear();
  activeTurnouts.clear();
  activeName.clear();
  if (name.empty()) return;
  File f = LittleFS.open(pathFor(name.c_str()), "r");
  if (!f) {
    eventLog.addf(LogKind::Error, "active layout '%s' not found", name.c_str());
    return;
  }
  JsonDocument filter;
  filter["marklin"] = true;
  filter["name"] = true;
  filter["pieces"][0]["id"] = true;
  filter["pieces"][0]["t"] = true;
  JsonDocument doc;
  const DeserializationError e = deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  if (e) {
    eventLog.addf(LogKind::Error, "layout '%s': %s", name.c_str(), e.c_str());
    return;
  }
  activeName = doc["name"] | name.c_str();
  for (JsonVariantConst t : doc["marklin"]["turnouts"].as<JsonArrayConst>()) {
    const std::string n = lowerStr(t["name"] | "");
    uint32_t uid = 0, uid2 = 0;
    if (n.empty()) continue;
    if (automation::parseAccessoryRef(t["addr"] | "", &uid)) activeAccessories.push_back({n, uid});
    if (automation::parseAccessoryRef(t["addr2"] | "", &uid2)) activeAccessories.push_back({n + "_2", uid2});
    if (!uid) continue;
    std::string article;
    const int piece = t["piece"] | -1;
    for (JsonVariantConst p : doc["pieces"].as<JsonArrayConst>())
      if ((p["id"] | -2) == piece) {
        article = p["t"] | "";
        break;
      }
    activeTurnouts.push_back({t["name"] | "", article, uid, uid2});
  }
  for (JsonVariantConst s : doc["marklin"]["sensors"].as<JsonArrayConst>()) {
    const uint32_t id = s["id"] | 0u;
    if (id == 0) continue;
    activeSensors.push_back({lowerStr(s["name"] | ""), id});
  }
  eventLog.addf(LogKind::Info, "active layout '%s': %u turnouts, %u sensors", name.c_str(),
                static_cast<unsigned>(activeAccessories.size()),
                static_cast<unsigned>(activeSensors.size()));
}

const std::vector<NamedAccessory>& accessories() { return activeAccessories; }
const std::vector<NamedSensor>& sensors() { return activeSensors; }
const std::vector<LayoutTurnout>& turnouts() { return activeTurnouts; }
const std::string& activeTitle() { return activeName; }

}  // namespace layouts
