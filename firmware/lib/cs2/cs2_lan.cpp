#include "cs2_lan.h"

#include <stdio.h>
#include <string.h>

#include "marklin_controller.h"

namespace cs2 {

using marklin::CanFrame;

void encodeFrame(const CanFrame& f, uint8_t out[kFrameSize]) {
  marklin::writeBe32(out, f.id & 0x1FFFFFFF);
  out[4] = f.dlc;
  memcpy(out + 5, f.data, 8);
}

bool decodeFrame(const uint8_t in[kFrameSize], CanFrame* f) {
  if (in[4] > 8) return false;
  f->id = marklin::readBe32(in) & 0x1FFFFFFF;
  f->dlc = in[4];
  memcpy(f->data, in + 5, 8);
  return true;
}

uint16_t crcCcitt(const uint8_t* data, size_t len, uint16_t seed) {
  uint16_t crc = seed;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

uint32_t adler32(const uint8_t* data, size_t len) {
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < len; i++) {
    a = (a + data[i]) % 65521;
    b = (b + a) % 65521;
  }
  return (b << 16) | a;
}

std::vector<uint8_t> zlibStore(const std::string& data) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data.data());
  const size_t n = data.size();
  std::vector<uint8_t> out;
  out.reserve(n + n / 65535 * 5 + 11);
  out.push_back(0x78);  // deflate, ventana de 32 KB
  out.push_back(0x01);  // sin diccionario; (0x78 << 8 | 0x01) % 31 == 0
  size_t pos = 0;
  do {
    const size_t len = n - pos > 65535 ? 65535 : n - pos;
    const bool last = pos + len == n;
    out.push_back(last ? 0x01 : 0x00);  // BFINAL + BTYPE=00 (stored)
    out.push_back(len & 0xFF);
    out.push_back(len >> 8);
    out.push_back(~len & 0xFF);
    out.push_back((~len >> 8) & 0xFF);
    out.insert(out.end(), p + pos, p + pos + len);
    pos += len;
  } while (pos < n);
  const uint32_t adler = adler32(p, n);
  for (int s = 24; s >= 0; s -= 8) out.push_back((adler >> s) & 0xFF);
  return out;
}

std::vector<CanFrame> configStream(uint32_t requestId, const std::string& content) {
  std::vector<uint8_t> z = zlibStore(content);
  // Algunos receptores calculan el CRC sobre (longitud + 8) & ~7 bytes: si la
  // longitud ya es múltiplo de 8 cuentan 8 bytes de más. Un bloque "stored"
  // vacío (5 bytes) delante evita ese caso sin cambiar el contenido.
  if ((4 + z.size()) % 8 == 0) {
    const uint8_t empty[5] = {0x00, 0x00, 0x00, 0xFF, 0xFF};
    z.insert(z.begin() + 2, empty, empty + 5);
  }
  std::vector<uint8_t> buf(4 + z.size());
  marklin::writeBe32(buf.data(), static_cast<uint32_t>(content.size()));
  memcpy(buf.data() + 4, z.data(), z.size());
  const uint32_t length = static_cast<uint32_t>(buf.size());
  buf.resize((buf.size() + 7) / 8 * 8, 0);

  const uint32_t id = (requestId & 0x1FFEFFFF & ~(0xFFu << 17)) |
                      (static_cast<uint32_t>(marklin::CMD_CONFIG_DATA_STREAM) << 17);
  std::vector<CanFrame> frames;
  frames.reserve(1 + buf.size() / 8);
  CanFrame head;
  head.id = id;
  head.dlc = 6;
  marklin::writeBe32(head.data, length);
  marklin::writeBe16(head.data + 4, crcCcitt(buf.data(), buf.size()));
  frames.push_back(head);
  for (size_t i = 0; i < buf.size(); i += 8) {
    CanFrame f;
    f.id = id;
    f.dlc = 8;
    memcpy(f.data, buf.data() + i, 8);
    frames.push_back(f);
  }
  return frames;
}

// ---- Ficheros .cs2 -----------------------------------------------------------
// Formato de texto de la CS2: secciones sin sangría, valores " .clave=valor"
// y subvalores " ..clave=valor". Solo saltos de línea LF (con CR+LF algunas
// apps no los leen).

namespace {

void line(std::string* s, const char* text) {
  *s += text;
  *s += '\n';
}

void linef(std::string* s, const char* fmt, unsigned long v) {
  char buf[48];
  snprintf(buf, sizeof(buf), fmt, v);
  line(s, buf);
}

// Los nombres van en una línea: sin saltos ni caracteres de control.
std::string clean(const std::string& in) {
  std::string out;
  for (char c : in) out += (static_cast<unsigned char>(c) < 0x20) ? ' ' : c;
  return out;
}

void keyText(std::string* s, const char* key, const std::string& value) {
  *s += key;
  *s += clean(value);
  *s += '\n';
}

}  // namespace

std::string lokomotiveCs2(const std::vector<LocoEntry>& locos) {
  std::string s;
  line(&s, "[lokomotive]");
  line(&s, "version");
  line(&s, " .minor=3");
  line(&s, "session");
  line(&s, " .id=1");
  for (const auto& l : locos) {
    marklin::Protocol proto;
    uint32_t address;
    if (!marklin::decodeLocoUid(l.uid, &proto, &address)) continue;
    line(&s, "lokomotive");
    keyText(&s, " .name=", l.name);
    linef(&s, " .uid=0x%lx", l.uid);
    linef(&s, " .adresse=0x%lx", address);
    int functions = 16;
    switch (proto) {
      case marklin::Protocol::MFX:
        line(&s, " .typ=mfx");
        linef(&s, " .sid=0x%lx", address);
        break;
      case marklin::Protocol::DCC:
        line(&s, " .typ=dcc");
        break;
      default:
        line(&s, " .typ=mm2_prg");
        functions = 5;  // MM2: luz + F1..F4
        break;
    }
    line(&s, " .av=6");
    line(&s, " .bv=3");
    line(&s, " .volume=100");
    line(&s, " .tachomax=200");
    line(&s, " .vmax=255");
    line(&s, " .vmin=0");
    if (!l.fn.empty()) functions = static_cast<int>(l.fn.size());
    for (int n = 0; n < functions; n++) {
      const unsigned long typ = !l.fn.empty() ? l.fn[n] : n == 0 ? 1 : 50 + n;
      line(&s, " .funktionen");
      linef(&s, " ..nr=%lu", n);
      // 1 = luces; 50 + n = símbolo genérico "Fn"; 0x80 = momentánea.
      if (typ) linef(&s, " ..typ=%lu", typ);
    }
  }
  return s;
}

std::string magnetartikelCs2(const std::vector<ArticleEntry>& articles) {
  std::string s;
  line(&s, "[magnetartikel]");
  line(&s, "version");
  line(&s, " .minor=1");
  for (const auto& a : articles) {
    line(&s, "artikel");
    linef(&s, " .id=%lu", a.id);
    keyText(&s, " .name=", a.name);
    keyText(&s, " .typ=", a.typ);
    line(&s, " .stellung=0");
    line(&s, " .schaltzeit=200");
    if (!a.dcc) line(&s, " .decoder=mehr_neu");
    line(&s, a.dcc ? " .dectyp=dcc" : " .dectyp=mm2");
  }
  return s;
}

std::string gleisbildCs2(const std::string& pageName) {
  std::string s;
  line(&s, "[gleisbild]");
  line(&s, "version");
  line(&s, " .major=1");
  line(&s, "groesse");
  line(&s, "zuletztBenutzt");
  keyText(&s, " .name=", pageName);
  line(&s, "seite");
  keyText(&s, " .name=", pageName);
  return s;
}

std::string gleisbildPageCs2(const Model& model) {
  if (!model.trackPlan.empty()) return model.trackPlan;
  std::string s;
  line(&s, "[gleisbildseite]");
  line(&s, "version");
  line(&s, " .major=1");
  // Rejilla: id = columna << 8 | fila. Cuatro desvíos por fila, separados
  // para que se lean los nombres.
  int row = 1;
  for (size_t i = 0; i < model.articles.size(); i++) {
    const auto& a = model.articles[i];
    const int x = 1 + static_cast<int>(i % 4) * 4;
    row = 1 + static_cast<int>(i / 4) * 2;
    line(&s, "element");
    linef(&s, " .id=0x%lx", static_cast<unsigned long>((x << 8) | row));
    keyText(&s, " .typ=", a.typ);
    linef(&s, " .artikel=%lu", a.id);
    keyText(&s, " .text=", a.name);
  }
  int first = model.articles.empty() ? 1 : row + 2;
  for (size_t i = 0; i < model.contacts.size(); i++) {
    const auto& c = model.contacts[i];
    const int x = 1 + static_cast<int>(i % 6) * 3;
    const int y = first + static_cast<int>(i / 6) * 2;
    line(&s, "element");
    linef(&s, " .id=0x%lx", static_cast<unsigned long>((x << 8) | y));
    line(&s, " .typ=s88kontakt");
    linef(&s, " .artikel=%lu", c.id);
    keyText(&s, " .text=", c.name.empty() ? std::to_string(c.id) : c.name);
  }
  return s;
}

std::string geraetVrs(uint32_t uid) {
  std::string s;
  line(&s, "[geraet]");
  line(&s, "version");
  line(&s, " .minor=1");
  line(&s, "geraet");
  linef(&s, " .sernum=%lu", uid & 0xFFFF);
  line(&s, " .hardvers=ESP32,3");
  return s;
}

std::string prefsCs2(const Model& model) {
  std::string s;
  auto entry = [&](const char* key, const std::string& value) {
    line(&s, " .entry");
    s += " ..key=";
    s += key;
    s += "\n ..value=";
    s += value;
    s += '\n';
  };
  line(&s, "[Preferences]");
  line(&s, "version");
  line(&s, " .major=0");
  line(&s, " .minor=1");
  line(&s, "page");
  entry("Vergabe", "automatisch");
  entry("IpAdresse", model.ip);
  entry("NetzMaske", "255.255.255.0");
  line(&s, "page");
  entry("Version", "4.2.13");
  line(&s, "page");
  entry("Sprache", "de");
  line(&s, "page");
  entry("SerNum", std::to_string(model.uid & 0xFFFF));
  entry("GfpUid", std::to_string(model.uid));
  entry("GuiUid", std::to_string(model.uid + 1));
  entry("HardVers", "3.1");
  return s;
}

namespace {

std::string stateFile(const char* section, const char* version) {
  std::string s;
  line(&s, section);
  line(&s, "version");
  line(&s, version);
  return s;
}

}  // namespace

bool configFile(const std::string& request, const Model& model, std::string* content) {
  if (request == "loks") {
    *content = lokomotiveCs2(model.locos);
  } else if (request == "mags") {
    *content = magnetartikelCs2(model.articles);
  } else if (request == "gbs") {
    *content = gleisbildCs2(model.pageName);
  } else if (request.compare(0, 4, "gbs-") == 0) {
    // Una sola página (la 0).
    if (request != "gbs-0") return false;
    *content = gleisbildPageCs2(model);
  } else if (request == "prefs") {
    *content = prefsCs2(model);
  } else if (request == "geraet") {
    *content = geraetVrs(model.uid);
  } else if (request == "fs") {
    *content = stateFile("[fahrstrassen]", " .minor=4");
  } else if (request == "lokstat") {
    *content = stateFile("[lokstatus]", " .minor=1");
  } else if (request == "magstat") {
    *content = stateFile("[magnetartikel]", " .minor=1");
  } else if (request == "gbsstat") {
    *content = stateFile("[gleisbild]", " .minor=1");
  } else if (request == "fsstat") {
    *content = stateFile("[fahrstrassen]", " .minor=1");
  } else {
    return false;
  }
  return true;
}

// ---- Pasarela ----------------------------------------------------------------

void Gateway::setUid(uint32_t uid) {
  uid_ = uid;
  hash_ = marklin::hashFromUid(uid);
}

CanFrame Gateway::pingReply() const {
  CanFrame f;
  f.id = marklin::makeId(marklin::CMD_PING, true, hash_);
  f.dlc = 8;
  marklin::writeBe32(f.data, uid_);
  f.data[4] = 4;  // versión de software 4.2
  f.data[5] = 2;
  marklin::writeBe16(f.data + 6, kDeviceTypeCentralStation);
  return f;
}

bool Gateway::handleClientFrame(const CanFrame& f, const std::function<void(Model*)>& fillModel,
                                std::vector<CanFrame>* replies) {
  const uint8_t cmd = marklin::commandOf(f);
  const bool response = marklin::isResponse(f);

  if (cmd == marklin::CMD_PING) {
    // Somos la central: contestamos y dejamos que el ping llegue también a
    // la Gleisbox, la MS2, etc. para que la app los vea. Muchas apps no
    // mandan una petición de ping sino que se anuncian con su propia
    // respuesta (tipo 0xEEEE, "CS2 software") y esperan a que la central
    // conteste: también les respondemos.
    if (!response || (f.dlc >= 4 && marklin::readBe32(f.data) != uid_)) replies->push_back(pingReply());
    return true;
  }

  if (cmd == marklin::CMD_CONFIG_DATA_REQUEST && !response && f.dlc == 8) {
    char name[9];
    memcpy(name, f.data, 8);
    name[8] = 0;
    Model model;
    std::string content;
    if (fillModel) fillModel(&model);
    if (!configFile(name, model, &content)) return true;  // quizá lo conteste otro equipo
    configRequests_++;
    CanFrame ack = f;
    ack.id |= 0x10000;  // bit de respuesta
    replies->push_back(ack);
    const std::vector<CanFrame> stream = configStream(f.id, content);
    replies->insert(replies->end(), stream.begin(), stream.end());
    return false;
  }

  return true;
}

bool emulateTrackResponse(const CanFrame& req, const marklin::Controller& c, uint16_t hash, CanFrame* out) {
  using namespace marklin;
  if (isResponse(req)) return false;
  const uint8_t cmd = commandOf(req);
  *out = req;
  out->id = makeId(cmd, true, hash);
  const LocoState* l = req.dlc >= 4 ? c.findLoco(readBe32(req.data)) : nullptr;
  switch (cmd) {
    case CMD_SYSTEM:
      if (req.dlc == 4) {  // consulta de estado: 1 = vía con corriente
        out->dlc = 5;
        out->data[4] = c.power() == PowerState::On ? 1 : 0;
        return true;
      }
      return req.dlc >= 5 && (req.data[4] == SYS_STOP || req.data[4] == SYS_GO || req.data[4] == SYS_HALT ||
                              req.data[4] == SYS_LOCO_EMERGENCY);
    case CMD_LOCO_SPEED:
      if (req.dlc == 4) {
        out->dlc = 6;
        writeBe16(out->data + 4, l ? l->speed : 0);
        return true;
      }
      return req.dlc == 6;
    case CMD_LOCO_DIRECTION:
      if (req.dlc == 4) {
        out->dlc = 5;
        out->data[4] = l ? l->direction : 0;
        return true;
      }
      return req.dlc == 5;
    case CMD_LOCO_FUNCTION:
      if (req.dlc == 5) {
        out->dlc = 6;
        out->data[5] = l && req.data[4] < 32 ? (l->functions >> req.data[4]) & 1 : 0;
        return true;
      }
      return req.dlc >= 6;
    case CMD_ACCESSORY:
      return req.dlc >= 6;
    default:
      return false;
  }
}

CanFrame s88Event(uint16_t hash, uint32_t id, bool active) {
  CanFrame f;
  f.id = marklin::makeId(marklin::CMD_S88_EVENT, true, hash);
  f.dlc = 8;
  marklin::writeBe16(f.data, static_cast<uint16_t>(id / 1000));
  marklin::writeBe16(f.data + 2, static_cast<uint16_t>(id % 1000));
  f.data[4] = active ? 0 : 1;  // estado anterior
  f.data[5] = active ? 1 : 0;  // estado nuevo
  return f;
}

}  // namespace cs2
