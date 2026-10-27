#include "ms2_import.h"

#include <stdlib.h>
#include <string.h>

#include "cs2_lan.h"

namespace cs2 {

using marklin::CanFrame;

namespace {

std::string rtrim(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\t' || s.back() == '\0'))
    s.pop_back();
  return s;
}

bool startsWith(const std::string& s, const char* prefix) {
  return s.compare(0, strlen(prefix), prefix) == 0;
}

// Valor tras "clave=" (o "" si la línea no es esa clave).
bool keyValue(const std::string& line, const char* key, std::string* value) {
  if (!startsWith(line, key)) return false;
  *value = line.substr(strlen(key));
  return true;
}

uint32_t number(const std::string& s) { return static_cast<uint32_t>(strtoul(s.c_str(), nullptr, 0)); }

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> out;
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    if (nl == std::string::npos) nl = text.size();
    std::string l = text.substr(pos, nl - pos);
    if (!l.empty() && l.back() == '\r') l.pop_back();
    out.push_back(l);
    pos = nl + 1;
  }
  return out;
}

}  // namespace

std::string stripMs2Spaces(const std::string& in) {
  // Solo si el texto viene con el relleno de la MS2: tras cada salto de
  // línea, espacios hasta el siguiente múltiplo de 8.
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] != '\n') continue;
    for (size_t j = i + 1; j < in.size() && j % 8; j++)
      if (in[j] != ' ' && in[j] != '\0') return in;
  }
  std::string out;
  bool nl = false;
  for (size_t i = 0; i < in.size(); i++) {
    if (nl && i % 8) continue;
    nl = in[i] == '\n';
    out += in[i];
  }
  return out;
}

std::vector<std::string> parseLocoNames(const std::string& text, int* total) {
  std::vector<std::string> names;
  if (total) *total = -1;
  std::string v;
  for (const std::string& l : lines(text)) {
    if (keyValue(l, " .name=", &v)) {
      v = rtrim(v);
      if (!v.empty()) names.push_back(v);
    } else if (keyValue(l, " .wert=", &v) && total) {
      *total = static_cast<int>(number(v));
    }
  }
  return names;
}

std::vector<ImportedLoco> parseLocos(const std::string& text) {
  std::vector<ImportedLoco> out;
  struct Work {
    ImportedLoco loco;
    std::string typ;
    uint32_t address = 0;
    uint32_t sid = 0;
    std::vector<bool> moment;
  };
  std::vector<Work> work;
  Work* cur = nullptr;
  int fn = -1;
  bool inFn = false;

  auto setFn = [&](int n, uint8_t typ) {
    if (!cur || n < 0 || n > 31) return;
    if (cur->loco.fn.size() <= static_cast<size_t>(n)) cur->loco.fn.resize(n + 1, 0);
    cur->loco.fn[n] = typ;
  };
  auto setMoment = [&](int n) {
    if (!cur || n < 0 || n > 31) return;
    if (cur->moment.size() <= static_cast<size_t>(n)) cur->moment.resize(n + 1, false);
    cur->moment[n] = true;
  };
  auto newLoco = [&]() {
    work.push_back(Work());
    cur = &work.back();
    fn = -1;
    inFn = false;
  };

  std::string v;
  for (const std::string& raw : lines(text)) {
    const std::string l = rtrim(raw);
    if (l.empty()) continue;
    if (l[0] != ' ') {  // nivel 0: secciones
      if (l == "lokomotive" || l == "lok") {
        newLoco();
      } else {
        cur = nullptr;
        inFn = false;
      }
      continue;
    }
    if (startsWith(l, " ..")) {  // nivel 2: datos de una función
      if (!inFn || !cur) continue;
      if (keyValue(l, " ..nr=", &v)) {
        fn = static_cast<int>(number(v) & 0x1F);
      } else if (keyValue(l, " ..typ=", &v) || keyValue(l, " ..typ2=", &v)) {
        setFn(fn, static_cast<uint8_t>(number(v) & 0xFF));
      } else if (keyValue(l, " ..dauer=", &v) || keyValue(l, " ..dauer2=", &v)) {
        if (strtol(v.c_str(), nullptr, 0) != 0) setMoment(fn);
      }
      continue;
    }
    // Nivel 1.
    if (l == " .fkt" || l == " .fkt2" || l == " .funktionen" || l == " .funktionen_2") {
      if (!cur) newLoco();
      fn++;
      inFn = true;
      continue;
    }
    inFn = false;
    if (!cur) {
      if (!startsWith(l, " .name=") && !startsWith(l, " .uid=")) continue;
      newLoco();
    }
    if (keyValue(l, " .name=", &v)) cur->loco.name = v;
    else if (keyValue(l, " .uid=", &v)) cur->loco.uid = number(v);
    else if (keyValue(l, " .mfxuid=", &v)) cur->loco.mfxUid = number(v);
    else if (keyValue(l, " .adresse=", &v)) cur->address = number(v);
    else if (keyValue(l, " .sid=", &v)) cur->sid = number(v);
    else if (keyValue(l, " .typ=", &v)) cur->typ = v;
  }

  for (Work& w : work) {
    ImportedLoco& l = w.loco;
    if (l.uid == 0) {
      if (w.typ == "mfx") l.uid = marklin::UID_MFX_BASE + (w.sid ? w.sid : w.address);
      else if (w.typ == "dcc") l.uid = marklin::UID_DCC_BASE + w.address;
      else if (w.typ == "sx1") l.uid = marklin::UID_SX1_BASE + w.address;
      else l.uid = w.address;  // mm2_prg, mm2_dil8...
    }
    for (size_t n = 0; n < w.moment.size() && n < l.fn.size(); n++)
      if (w.moment[n] && l.fn[n] && l.fn[n] < kFnMomentary) l.fn[n] |= kFnMomentary;
    while (!l.fn.empty() && l.fn.back() == 0) l.fn.pop_back();
    if (l.uid) out.push_back(l);
  }
  return out;
}

// ---- Recepción de la secuencia ---------------------------------------------------

bool StreamReceiver::feed(const CanFrame& f) {
  if (marklin::commandOf(f) != marklin::CMD_CONFIG_DATA_STREAM) return false;
  if (f.dlc == 6 || f.dlc == 7) {
    length_ = marklin::readBe32(f.data);
    crc_ = marklin::readBe16(f.data + 4);
    buf_.clear();
    active_ = length_ > 0 && length_ < 64 * 1024;
    return false;
  }
  if (f.dlc != 8 || !active_) return false;
  buf_.append(reinterpret_cast<const char*>(f.data), 8);
  if (buf_.size() < length_) return false;
  active_ = false;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(buf_.data());
  crcOk_ = crcCcitt(p, buf_.size()) == crc_ || crcCcitt(p, length_) == crc_;
  const std::string data = buf_.substr(0, length_);
  // Formato CS2: [tamaño BE32][zlib]. La MS2 manda texto sin comprimir.
  compressed_ = data.size() > 6 && data[0] == 0 && static_cast<uint8_t>(data[4]) == 0x78;
  text_ = compressed_ ? std::string() : stripMs2Spaces(rtrim(data));
  return true;
}

// ---- Importador --------------------------------------------------------------------

void Ms2Importer::start(uint16_t hash, bool v3, uint32_t nowMs) {
  hash_ = hash;
  v3_ = v3;
  sentMs_ = nowMs;
  retries_ = 0;
  total_ = -1;
  next_ = 0;
  names_.clear();
  locos_.clear();
  error_.clear();
  rx_.reset();
  state_ = State::List;
  requestList();
}

void Ms2Importer::request(const char* name, const std::vector<std::string>& extra) {
  rx_.reset();
  CanFrame f;
  f.id = marklin::makeId(marklin::CMD_CONFIG_DATA_REQUEST, false, hash_);
  f.dlc = 8;
  strncpy(reinterpret_cast<char*>(f.data), name, 8);
  if (send) send(f);
  for (const std::string& e : extra) {
    memset(f.data, 0, 8);
    memcpy(f.data, e.data(), e.size() < 8 ? e.size() : 8);
    if (send) send(f);
  }
}

void Ms2Importer::requestList() {
  if (v3_) {
    request("lokliste", {});
  } else {
    request("loknamen", {std::to_string(names_.size()) + " 2"});
  }
}

void Ms2Importer::requestInfo() {
  state_ = State::Info;
  if (next_ >= names_.size()) {
    state_ = State::Done;
    return;
  }
  const std::string& n = names_[next_];
  request("lokinfo", {n.substr(0, 8), n.size() > 8 ? n.substr(8, 8) : std::string()});
}

void Ms2Importer::handleFrame(const CanFrame& f, uint32_t nowMs) {
  if (!busy() || !rx_.feed(f)) return;
  sentMs_ = nowMs;
  retries_ = 0;
  if (rx_.compressed()) return fail("the MS2 answered in an unsupported format");
  fileReceived(rx_.text());
}

void Ms2Importer::fileReceived(const std::string& text) {
  if (state_ == State::List) {
    int total = -1;
    const std::vector<std::string> got = parseLocoNames(text, &total);
    for (const std::string& n : got) {
      bool dup = false;
      for (const std::string& o : names_) dup = dup || o == n;
      if (!dup) names_.push_back(n);
    }
    if (total >= 0) total_ = total;
    if (!v3_ && !got.empty() && total_ > static_cast<int>(names_.size())) return requestList();
    next_ = 0;
    return requestInfo();
  }
  if (state_ == State::Info) {
    std::vector<ImportedLoco> parsed = parseLocos(text);
    if (!parsed.empty()) {
      ImportedLoco l = parsed.front();
      if (l.name.empty()) l.name = names_[next_];
      locos_.push_back(l);
    }
    next_++;
    requestInfo();
  }
}

void Ms2Importer::tick(uint32_t nowMs) {
  if (!busy() || nowMs - sentMs_ < kTimeoutMs) return;
  sentMs_ = nowMs;
  if (retries_ < 1) {
    retries_++;
    if (state_ == State::List) requestList();
    else requestInfo();
    return;
  }
  retries_ = 0;
  if (state_ == State::Info) {  // esta loco no contesta: siguiente
    next_++;
    requestInfo();
    return;
  }
  fail("the MS2 does not answer (is it connected to the Gleisbox?)");
}

void Ms2Importer::fail(const char* why) {
  error_ = why;
  state_ = State::Failed;
}

}  // namespace cs2
