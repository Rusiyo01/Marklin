#include "script_store.h"

#include <LittleFS.h>

#include <algorithm>

#include "default_scripts.h"
#include "event_log.h"

namespace scripts {

static const char* kDir = "/scripts";
static const char* kDefaultsMarker = "/scripts/.ejemplos";

static String pathFor(const String& name) { return String(kDir) + "/" + name + ".txt"; }

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
    if (!f.isDirectory() && n.endsWith(".txt")) out.push_back(n.substring(0, n.length() - 4));
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

bool write(const String& name, const String& content) {
  if (!validName(name) || content.length() > kMaxScriptSize) return false;
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  File f = LittleFS.open(pathFor(name), "w");
  if (!f) return false;
  const size_t n = f.print(content);
  f.close();
  return n == content.length();
}

bool remove(const String& name) {
  return validName(name) && LittleFS.remove(pathFor(name));
}

void installDefaults() {
  if (!LittleFS.exists(kDir)) LittleFS.mkdir(kDir);
  if (LittleFS.exists(kDefaultsMarker)) return;
  for (int i = 0; i < kDefaultScriptCount; i++) {
    const String name = kDefaultScripts[i].name;
    if (!exists(name)) write(name, kDefaultScripts[i].source);
  }
  File m = LittleFS.open(kDefaultsMarker, "w");
  if (m) {
    m.print("1");
    m.close();
  }
  eventLog.addf(LogKind::Info, "instalados %d scripts de ejemplo", kDefaultScriptCount);
}

}  // namespace scripts
