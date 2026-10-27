#include "inventory.h"

#include <LittleFS.h>

#include <set>

#include "app.h"
#include "event_log.h"
#include "ms2_import.h"

namespace inventory {

namespace {

cs2::Ms2Importer importer;
bool importRunning = false;
bool importAuto = false;
String status;
uint32_t changes = 1;

// Lectura automática programada (0 = ninguna).
uint32_t autoAtMs = 0;
uint32_t lastAutoMs = 0;
bool lastAutoValid = false;
// Tras dar de alta una loco mfx se vuelve a leer más tarde, por si la MS2 aún
// no había terminado de leer el decodificador.
bool followUp = false;
// Locos desconocidas que ya provocaron una lectura (para no repetir).
std::set<uint32_t> tried;

const marklin::DeviceInfo* ms2() {
  for (int i = 0; i < controller.deviceCount(); i++) {
    const marklin::DeviceInfo& d = controller.device(i);
    if (d.type >= 0x0030 && d.type <= 0x0034) return &d;
  }
  return nullptr;
}

bool registered(uint32_t uid) {
  for (const auto& l : config.locos)
    if (l.uid == uid) return true;
  return false;
}

void scheduleAuto(uint32_t delayMs) {
  if (!config.autoImport || importRunning) return;
  const uint32_t at = millis() + delayMs;
  if (!autoAtMs || static_cast<int32_t>(at - autoAtMs) < 0) autoAtMs = at ? at : 1;
}

void merge() {
  int added = 0, updated = 0;
  String names;
  for (const cs2::ImportedLoco& m : importer.locos()) {
    marklin::Protocol p;
    uint32_t addr;
    if (!marklin::decodeLocoUid(m.uid, &p, &addr)) continue;
    LocoConfig* l = nullptr;
    for (auto& c : config.locos)
      if (c.uid == m.uid) l = &c;
    if (l) {
      bool changed = false;
      // Los datos del decodificador mfx mandan; en MM2/DCC no se tocan las
      // funciones que ya se hayan configurado a mano.
      if (!m.fn.empty() && (l->fn.empty() || p == marklin::Protocol::MFX) && l->fn != m.fn) {
        l->fn = m.fn;
        changed = true;
      }
      if (m.mfxUid && l->mfxUid != m.mfxUid) {
        l->mfxUid = m.mfxUid;
        changed = true;
      }
      if (l->label.empty() && !m.name.empty()) {
        l->label = m.name;
        changed = true;
      }
      updated += changed;
      continue;
    }
    LocoConfig c;
    c.uid = m.uid;
    c.label = m.name.substr(0, 60);
    c.name = scriptName(m.name);
    c.fn = m.fn;
    c.mfxUid = m.mfxUid;
    config.locos.push_back(c);
    controller.setLocoName(c.uid, c.name.c_str());
    added++;
    if (names.length()) names += ", ";
    names += m.name.c_str();
  }
  if (added || updated) {
    saveConfig(config);
    touch();
  }
  status = "MS2: " + String(importer.locos().size()) + " locos read, " + String(added) + " new, " +
           String(updated) + " updated";
  if (added) status += ": " + names;
  if (added || updated || !importAuto) eventLog.add(LogKind::Info, status.c_str());
}

void begin(bool automatic) {
  const marklin::DeviceInfo* d = ms2();
  importer.start(controller.hash(), !d || d->versionHigh >= 3, millis());
  importRunning = true;
  importAuto = automatic;
  status = "Reading the loco list from the MS2...";
  if (!automatic) eventLog.add(LogKind::Info, "MS2: reading its locos");
}

}  // namespace

void setup() {
  LittleFS.mkdir("/photos");
  importer.send = [](const marklin::CanFrame& f) { return controller.sendFrame(f); };
}

void loop(uint32_t now) {
  importer.tick(now);
  if (importRunning) {
    if (importer.state() == cs2::Ms2Importer::State::Info && importer.current() < importer.names().size())
      status = "Reading '" + String(importer.names()[importer.current()].c_str()) + "' (" +
               String(importer.current() + 1) + "/" + String(importer.names().size()) + ")...";
    if (!importer.busy()) {
      importRunning = false;
      if (importer.state() == cs2::Ms2Importer::State::Done) {
        merge();
        if (followUp) {
          followUp = false;
          scheduleAuto(45000);
        }
      } else {
        status = String("MS2: ") + importer.error().c_str();
        if (!importAuto) eventLog.add(LogKind::Error, status.c_str());
      }
    }
  }
  if (autoAtMs && static_cast<int32_t>(now - autoAtMs) >= 0) {
    autoAtMs = 0;
    if (!importRunning && ms2() && config.autoImport) {
      lastAutoMs = now;
      lastAutoValid = true;
      begin(true);
    }
  }
}

void onBusFrame(const marklin::CanFrame& f, bool tx) {
  if (tx) return;
  if (importRunning) importer.handleFrame(f, millis());
  if (!config.autoImport) return;
  const uint8_t cmd = marklin::commandOf(f);
  if (cmd == marklin::CMD_MFX_BIND) {
    // La MS2 acaba de dar de alta una loco mfx: lee del decodificador su
    // nombre y funciones durante unos segundos. Leemos después (y otra vez
    // más tarde, por si tarda).
    if (!autoAtMs && !importRunning)
      eventLog.add(LogKind::Info, "MS2: new mfx loco on the track; it will be copied to the inventory");
    scheduleAuto(20000);
    followUp = true;
    tried.clear();
    return;
  }
  if ((cmd == marklin::CMD_LOCO_SPEED || cmd == marklin::CMD_LOCO_DIRECTION ||
       cmd == marklin::CMD_LOCO_FUNCTION) && !marklin::isResponse(f) && f.dlc >= 4) {
    const uint32_t uid = marklin::readBe32(f.data);
    if (!marklin::isLocoUid(uid) || registered(uid) || tried.count(uid)) return;
    if (lastAutoValid && millis() - lastAutoMs < 30000) return;
    tried.insert(uid);
    scheduleAuto(1500);
  }
}

bool startImport(String* error) {
  if (importRunning) {
    *error = "the locos are already being read";
    return false;
  }
  if (!ms2()) {
    *error = "there is no MS2 on the bus (press 'Find devices' in Settings)";
    return false;
  }
  begin(false);
  return true;
}

bool importBusy() { return importRunning; }
String importStatus() { return status; }
uint32_t seq() { return changes; }
void touch() { changes++; }

String photoPath(uint32_t uid) {
  char p[32];
  snprintf(p, sizeof(p), "/photos/%08lx.jpg", static_cast<unsigned long>(uid));
  return p;
}

bool hasPhoto(uint32_t uid) { return LittleFS.exists(photoPath(uid)); }

void removePhoto(uint32_t uid) {
  if (LittleFS.remove(photoPath(uid))) touch();
}

void renamePhoto(uint32_t from, uint32_t to) {
  if (from == to || !hasPhoto(from)) return;
  LittleFS.remove(photoPath(to));
  LittleFS.rename(photoPath(from), photoPath(to));
  touch();
}

std::string scriptName(const std::string& label) {
  std::string base;
  for (size_t i = 0; i < label.size() && base.size() < 24; i++) {
    unsigned char c = static_cast<unsigned char>(label[i]);
    if (c == 0xC3 && i + 1 < label.size()) {  // vocales con tilde, ñ, ü (UTF-8)
      const unsigned char n = static_cast<unsigned char>(label[++i]) | 0x20;
      c = (n >= 0xA0 && n <= 0xA5) ? 'a' : (n >= 0xA8 && n <= 0xAB) ? 'e' : (n >= 0xAC && n <= 0xAF) ? 'i'
        : (n >= 0xB2 && n <= 0xB6) ? 'o' : (n >= 0xB9 && n <= 0xBC) ? 'u' : n == 0xB1 ? 'n' : '_';
    } else if (c >= 'A' && c <= 'Z') {
      c = static_cast<unsigned char>(c - 'A' + 'a');
    } else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
      c = '_';
    }
    if (c == '_' && (base.empty() || base.back() == '_')) continue;
    base += static_cast<char>(c);
  }
  while (!base.empty() && base.back() == '_') base.pop_back();
  if (base.empty() || !(base[0] >= 'a' && base[0] <= 'z')) base = "loco_" + base;
  if (base.back() == '_') base.pop_back();
  std::string name = base;
  for (int n = 2;; n++) {
    bool used = false;
    for (const auto& l : config.locos) used = used || l.name == name;
    if (!used) return name;
    name = base + "_" + std::to_string(n);
  }
}

}  // namespace inventory
