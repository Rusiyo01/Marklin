#include <string.h>

#include <string>
#include <vector>

#include "cs2_lan.h"
#include "minitest.h"

using namespace marklin;

namespace {

// Reconstruye el fichero a partir de las tramas, como hace una app:
// comprueba longitud, CRC, cabecera zlib, bloques "stored" y Adler-32.
bool decodeStream(const std::vector<CanFrame>& frames, uint32_t expectedId, std::string* out) {
  if (frames.size() < 2) return false;
  const CanFrame& head = frames[0];
  if (head.id != expectedId || head.dlc != 6) return false;
  const uint32_t length = readBe32(head.data);
  const uint16_t crc = readBe16(head.data + 4);
  std::vector<uint8_t> buf;
  for (size_t i = 1; i < frames.size(); i++) {
    if (frames[i].id != expectedId || frames[i].dlc != 8) return false;
    buf.insert(buf.end(), frames[i].data, frames[i].data + 8);
  }
  if (buf.size() % 8 || buf.size() < length || buf.size() - length >= 8) return false;
  if (cs2::crcCcitt(buf.data(), buf.size()) != crc) return false;
  const uint32_t inflated = readBe32(buf.data());
  size_t p = 4;
  if (buf[p] != 0x78 || ((buf[p] << 8) | buf[p + 1]) % 31) return false;
  p += 2;
  std::string data;
  for (;;) {
    const uint8_t hdr = buf[p++];
    if ((hdr & 0x06) != 0) return false;  // solo bloques stored
    const uint16_t len = buf[p] | (buf[p + 1] << 8);
    const uint16_t nlen = buf[p + 2] | (buf[p + 3] << 8);
    if (static_cast<uint16_t>(~len) != nlen) return false;
    p += 4;
    data.append(reinterpret_cast<const char*>(&buf[p]), len);
    p += len;
    if (hdr & 1) break;
  }
  const uint32_t adler = readBe32(&buf[p]);
  if (adler != cs2::adler32(reinterpret_cast<const uint8_t*>(data.data()), data.size())) return false;
  if (p + 4 != length) return false;
  if (data.size() != inflated) return false;
  *out = data;
  return true;
}

CanFrame request(const char* name, uint16_t hash) {
  CanFrame f;
  f.id = makeId(CMD_CONFIG_DATA_REQUEST, false, hash);
  f.dlc = 8;
  strncpy(reinterpret_cast<char*>(f.data), name, 8);
  return f;
}

bool contains(const std::string& s, const char* needle) { return s.find(needle) != std::string::npos; }

void sampleModel(cs2::Model* m) {
  m->locos.push_back({"Cocodrilo SBB", locoUid(Protocol::MM2, 24), {}});
  m->locos.push_back({"BR 111", locoUid(Protocol::MFX, 5), {}});
  m->locos.push_back({"Vapor", locoUid(Protocol::DCC, 3), {}});
  m->articles.push_back({1, "entrada", "linksweiche", false});
  m->articles.push_back({2, "salida", "rechtsweiche", true});
  m->contacts.push_back({3, "anden"});
}

}  // namespace

TEST(cs2_trama_13_bytes_ida_y_vuelta) {
  CanFrame f;
  f.id = 0x00084711;
  f.dlc = 6;
  const uint8_t data[6] = {0, 0, 0, 24, 1, 0xF4};
  memcpy(f.data, data, 6);
  uint8_t raw[13];
  cs2::encodeFrame(f, raw);
  const uint8_t expected[13] = {0x00, 0x08, 0x47, 0x11, 6, 0, 0, 0, 24, 1, 0xF4, 0, 0};
  CHECK(memcmp(raw, expected, 13) == 0);
  CanFrame back;
  CHECK(cs2::decodeFrame(raw, &back));
  CHECK(back.id == f.id && back.dlc == 6 && memcmp(back.data, f.data, 8) == 0);
  raw[4] = 9;
  CHECK(!cs2::decodeFrame(raw, &back));
}

TEST(cs2_crc_y_adler) {
  const char* s = "123456789";
  CHECK(cs2::crcCcitt(reinterpret_cast<const uint8_t*>(s), 9) == 0x29B1);  // CRC-16/CCITT-FALSE
  CHECK(cs2::adler32(reinterpret_cast<const uint8_t*>("Wikipedia"), 9) == 0x11E60398);
}

TEST(cs2_zlib_bloques_grandes) {
  std::string big(70000, 'x');
  const std::vector<uint8_t> z = cs2::zlibStore(big);
  CHECK(z.size() == big.size() + 2 + 2 * 5 + 4);  // cabecera, 2 bloques, Adler
  CHECK(z[2] == 0x00);           // primer bloque no final
  CHECK(z[2 + 5 + 65535] == 1);  // segundo bloque final
  CHECK(cs2::zlibStore("").size() == 2 + 5 + 4);
}

TEST(cs2_ping_responde_como_central) {
  cs2::Gateway gw(0x43533201);
  std::vector<CanFrame> replies;
  CanFrame ping;
  ping.id = makeId(CMD_PING, false, 0x4711);
  ping.dlc = 0;
  CHECK(gw.handleClientFrame(ping, nullptr, &replies));  // también va al bus
  CHECK(replies.size() == 1);
  const CanFrame& r = replies[0];
  CHECK(commandOf(r) == CMD_PING && isResponse(r) && r.dlc == 8);
  CHECK(readBe32(r.data) == 0x43533201);
  CHECK(readBe16(r.data + 6) == 0xFFFF);
}

TEST(cs2_app_que_se_anuncia_recibe_el_ping_de_la_central) {
  // RailControl Pro y otras apps se anuncian con una respuesta de ping
  // (tipo 0xEEEE, "CS2 software") en vez de preguntar.
  cs2::Gateway gw(0x43533201);
  std::vector<CanFrame> replies;
  CanFrame hello;
  hello.id = makeId(CMD_PING, true, 0x4711);
  hello.dlc = 8;
  writeBe32(hello.data, 0x4F5910DF);
  hello.data[4] = 1;
  hello.data[5] = 4;
  writeBe16(hello.data + 6, 0xEEEE);
  CHECK(gw.handleClientFrame(hello, nullptr, &replies));
  CHECK(replies.size() == 1);
  CHECK(commandOf(replies[0]) == CMD_PING && isResponse(replies[0]));
  CHECK(readBe16(replies[0].data + 6) == 0xFFFF);
}

TEST(cs2_ordenes_de_la_app_van_al_bus) {
  cs2::Gateway gw;
  std::vector<CanFrame> replies;
  CanFrame speed = buildSpeed(0x4711, locoUid(Protocol::MM2, 24), 500);
  CHECK(gw.handleClientFrame(speed, nullptr, &replies));
  CHECK(replies.empty());
}

TEST(cs2_peticion_de_locos) {
  cs2::Gateway gw;
  std::vector<CanFrame> replies;
  const CanFrame req = request("loks", 0x4711);
  CHECK(!gw.handleClientFrame(req, sampleModel, &replies));
  CHECK(replies.size() > 3);
  CHECK(replies[0].id == (req.id | 0x10000) && memcmp(replies[0].data, "loks", 4) == 0);
  const std::vector<CanFrame> stream(replies.begin() + 1, replies.end());
  std::string file;
  CHECK(decodeStream(stream, makeId(CMD_CONFIG_DATA_STREAM, false, 0x4711), &file));
  CHECK(file.compare(0, 13, "[lokomotive]\n") == 0);
  CHECK(!contains(file, "\r"));
  CHECK(contains(file, "lokomotive\n .name=Cocodrilo SBB\n .uid=0x18\n .adresse=0x18\n .typ=mm2_prg\n"));
  CHECK(contains(file, " .uid=0x4005\n .adresse=0x5\n .typ=mfx\n .sid=0x5\n"));
  CHECK(contains(file, " .uid=0xc003\n .adresse=0x3\n .typ=dcc\n"));
  CHECK(contains(file, " ..nr=15\n ..typ=65\n"));
  CHECK(gw.configRequests() == 1);
}

TEST(cs2_desvios_y_plano) {
  cs2::Model m;
  sampleModel(&m);
  std::string f;
  CHECK(cs2::configFile("mags", m, &f));
  CHECK(contains(f, "artikel\n .id=1\n .name=entrada\n .typ=linksweiche\n"));
  CHECK(contains(f, " .id=2\n .name=salida\n .typ=rechtsweiche\n .stellung=0\n .schaltzeit=200\n .dectyp=dcc\n"));
  CHECK(cs2::configFile("gbs", m, &f));
  CHECK(contains(f, "seite\n .name=Layout\n"));
  CHECK(cs2::configFile("gbs-0", m, &f));
  CHECK(contains(f, "element\n .id=0x101\n .typ=linksweiche\n .artikel=1\n .text=entrada\n"));
  CHECK(contains(f, " .id=0x105\n .typ=rechtsweiche\n .artikel=2\n"));
  CHECK(contains(f, " .id=0x301\n .typ=s88kontakt\n .artikel=3\n .text=anden\n"));
  CHECK(!cs2::configFile("gbs-1", m, &f));
  CHECK(cs2::configFile("fs", m, &f) && f == "[fahrstrassen]\nversion\n .minor=4\n");
  CHECK(cs2::configFile("lokstat", m, &f));
  CHECK(!cs2::configFile("prefs", m, &f));
}

TEST(cs2_fichero_desconocido_va_al_bus) {
  cs2::Gateway gw;
  std::vector<CanFrame> replies;
  CHECK(gw.handleClientFrame(request("lokinfo", 0x4711), sampleModel, &replies));
  CHECK(replies.empty());
}

TEST(cs2_fichero_grande_en_muchas_tramas) {
  cs2::Model m;
  for (int i = 1; i <= 40; i++) m.locos.push_back({"Loco " + std::to_string(i), locoUid(Protocol::DCC, i), {}});
  const std::string content = cs2::lokomotiveCs2(m.locos);
  const std::vector<CanFrame> frames = cs2::configStream(makeId(CMD_CONFIG_DATA_REQUEST, false, 0x1234), content);
  std::string back;
  CHECK(decodeStream(frames, makeId(CMD_CONFIG_DATA_STREAM, false, 0x1234), &back));
  CHECK(back == content);
}

TEST(cs2_evento_s88) {
  const CanFrame f = cs2::s88Event(0x4711, 2005, true);
  CHECK(commandOf(f) == CMD_S88_EVENT && isResponse(f));
  CHECK(readBe16(f.data) == 2 && readBe16(f.data + 2) == 5 && f.data[4] == 0 && f.data[5] == 1);
}
