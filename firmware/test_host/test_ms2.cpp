#include <string.h>

#include <string>
#include <vector>

#include "cs2_lan.h"
#include "minitest.h"
#include "ms2_import.h"

using namespace marklin;

namespace {

// Texto con el relleno de la MS2 (como read_pipe de ms2-loco-list.c).
std::string ms2Pad(const std::string& text) {
  std::string out;
  for (char c : text) {
    out += c;
    if (c == '\n')
      while (out.size() % 8) out += ' ';
  }
  while (out.size() % 8) out += '\0';
  return out;
}

// Secuencia de configuración tal como la manda la MS2 (sin comprimir).
std::vector<CanFrame> ms2Stream(uint16_t hash, const std::string& text) {
  const std::string data = ms2Pad(text);
  std::vector<CanFrame> frames;
  CanFrame head;
  head.id = makeId(CMD_CONFIG_DATA_STREAM, false, hash);
  head.dlc = 6;
  writeBe32(head.data, static_cast<uint32_t>(data.size()));
  writeBe16(head.data + 4, cs2::crcCcitt(reinterpret_cast<const uint8_t*>(data.data()), data.size()));
  frames.push_back(head);
  for (size_t i = 0; i < data.size(); i += 8) {
    CanFrame f;
    f.id = head.id;
    f.dlc = 8;
    memcpy(f.data, data.data() + i, 8);
    frames.push_back(f);
  }
  return frames;
}

const char* kList =
    "[lokliste]\nlok\n .nr=0\n .name=BR 86 DB\nlok\n .nr=1\n .name=Cocodrilo SBB 16\n"
    "lok\n .nr=2\n .name=V 100\nnumloks\n .wert=3\n";

const char* kInfoBr86 =
    "[lokomotive]\nlokomotive\n .name=BR 86 DB\n .uid=0x4006\n .mfxuid=0x7cd3f9a1\n .adresse=0x6\n"
    " .typ=mfx\n .sid=0x6\n .icon=BR 86\n .vmax=255\n"
    " .fkt\n ..nr=0\n ..typ=1\n .fkt\n ..nr=1\n ..typ=5\n .fkt\n ..nr=2\n ..typ=10\n ..dauer=-1\n"
    " .fkt\n ..nr=3\n .fkt\n ..nr=4\n ..typ=8\n";

const char* kInfoCroc =
    "[lokomotive]\nlokomotive\n .name=Cocodrilo SBB 16\n .uid=0x18\n .adresse=0x18\n .typ=mm2_prg\n"
    " .fkt\n ..typ=1\n .fkt\n ..typ=7\n";

// MS2 simulada: contesta a las peticiones del importador.
struct FakeMs2 {
  std::vector<CanFrame> sent;
  std::vector<std::string> requests;
  bool mute = false;
  std::string pending;  // nombre de la petición en curso
  std::string name;     // bytes del nombre en lokinfo
  int extra = 0;
  std::vector<CanFrame> replies;

  bool send(const CanFrame& f) {
    sent.push_back(f);
    if (commandOf(f) != CMD_CONFIG_DATA_REQUEST || f.dlc != 8) return true;
    char buf[9] = {0};
    memcpy(buf, f.data, 8);
    if (extra > 0) {
      name.append(buf);
      if (--extra == 0) answer();
      return true;
    }
    pending = buf;
    requests.push_back(pending);
    name.clear();
    if (pending == "lokinfo") extra = 2;
    else if (pending == "loknamen") extra = 1;
    else answer();
    return true;
  }
  void answer() {
    if (mute) return;
    std::string text;
    if (pending == "lokliste") text = kList;
    else if (pending == "loknamen") {
      requests.back() += " " + name;
      if (name == "0 2") text = "[lokomotive]\nlok\n .nr=0\n .name=BR 86 DB\nlok\n .nr=1\n .name=V 100\nnumloks\n .wert=3\n";
      else text = "[lokomotive]\nlok\n .nr=2\n .name=E 18\nnumloks\n .wert=3\n";
    } else if (pending == "lokinfo") {
      requests.back() += " " + name;
      if (name == "BR 86 DB") text = kInfoBr86;
      else if (name == "Cocodrilo SBB 16") text = kInfoCroc;
      else return;  // V 100 no contesta
    }
    const std::vector<CanFrame> s = ms2Stream(0x5F3A, text);
    replies.insert(replies.end(), s.begin(), s.end());
  }
};

void pump(FakeMs2& ms2, cs2::Ms2Importer& imp, uint32_t* now) {
  for (int guard = 0; guard < 100 && imp.busy(); guard++) {
    std::vector<CanFrame> r;
    r.swap(ms2.replies);
    for (const auto& f : r) imp.handleFrame(f, *now);
    if (r.empty()) {
      *now += 1000;
      imp.tick(*now);
    }
  }
}

}  // namespace

TEST(ms2_quita_relleno) {
  CHECK(cs2::stripMs2Spaces(ms2Pad("lok\n .name=A\n")) == "lok\n .name=A\n");
  // Un texto sin el relleno de la MS2 no se toca.
  CHECK(cs2::stripMs2Spaces("lokomotive\n .name=A\n") == "lokomotive\n .name=A\n");
}

TEST(ms2_lista_de_nombres) {
  int total = 0;
  const auto names = cs2::parseLocoNames(kList, &total);
  CHECK(names.size() == 3 && names[1] == "Cocodrilo SBB 16" && total == 3);
}

TEST(ms2_ficha_de_loco_con_funciones) {
  const auto locos = cs2::parseLocos(kInfoBr86);
  CHECK(locos.size() == 1);
  const auto& l = locos[0];
  CHECK(l.name == "BR 86 DB" && l.uid == 0x4006 && l.mfxUid == 0x7cd3f9a1);
  CHECK(l.fn.size() == 5);
  CHECK(l.fn[0] == 1 && l.fn[1] == 5 && l.fn[2] == (10 | cs2::kFnMomentary) && l.fn[3] == 0 && l.fn[4] == 8);
  // Sin ..nr: numeración implícita; sin .uid: se calcula de la dirección.
  const auto croc = cs2::parseLocos("lokomotive\n .name=X\n .adresse=0x3\n .typ=dcc\n .fkt\n ..typ=1\n .fkt\n ..typ=7\n");
  CHECK(croc.size() == 1 && croc[0].uid == 0xC003 && croc[0].fn.size() == 2 && croc[0].fn[1] == 7);
}

TEST(ms2_lee_lokomotive_cs2_de_la_central) {
  std::vector<cs2::LocoEntry> in = {{"BR 86", 0x4006, {1, 5, 10 | cs2::kFnMomentary}}, {"Vapor", 0xC003, {}}};
  const auto out = cs2::parseLocos(cs2::lokomotiveCs2(in));
  CHECK(out.size() == 2);
  CHECK(out[0].name == "BR 86" && out[0].uid == 0x4006 && out[0].fn.size() == 3 && out[0].fn[2] == 138);
  CHECK(out[1].uid == 0xC003 && out[1].fn.size() == 16 && out[1].fn[0] == 1 && out[1].fn[3] == 53);
}

TEST(ms2_importa_todas_las_locos) {
  FakeMs2 ms2;
  cs2::Ms2Importer imp;
  imp.send = [&](const CanFrame& f) { return ms2.send(f); };
  uint32_t now = 1000;
  imp.start(0x4711, true, now);
  CHECK(imp.busy());
  pump(ms2, imp, &now);
  CHECK(imp.state() == cs2::Ms2Importer::State::Done);
  CHECK(imp.names().size() == 3);
  CHECK(imp.locos().size() == 2);  // V 100 no contesta: se salta
  if (imp.locos().size() != 2) return;
  CHECK(imp.locos()[0].name == "BR 86 DB" && imp.locos()[0].fn.size() == 5);
  CHECK(imp.locos()[1].uid == 24 && imp.locos()[1].fn.size() == 2);
  CHECK(ms2.requests.size() >= 4 && ms2.requests[0] == "lokliste" && ms2.requests[1] == "lokinfo BR 86 DB");
  CHECK(ms2.requests[2] == "lokinfo Cocodrilo SBB 16");
  // Todas las peticiones con nuestro hash.
  for (const auto& f : ms2.sent) CHECK(hashOf(f) == 0x4711 && commandOf(f) == CMD_CONFIG_DATA_REQUEST);
}

TEST(ms2_antigua_pide_por_paginas) {
  FakeMs2 ms2;
  cs2::Ms2Importer imp;
  imp.send = [&](const CanFrame& f) { return ms2.send(f); };
  uint32_t now = 0;
  imp.start(0x4711, false, now);
  pump(ms2, imp, &now);
  CHECK(imp.names().size() == 3 && imp.names()[2] == "E 18");
  CHECK(ms2.requests[0] == "loknamen 0 2" && ms2.requests[1] == "loknamen 2 2");
}

TEST(ms2_sin_respuesta_falla) {
  FakeMs2 ms2;
  ms2.mute = true;
  cs2::Ms2Importer imp;
  imp.send = [&](const CanFrame& f) { return ms2.send(f); };
  uint32_t now = 0;
  imp.start(0x4711, true, now);
  pump(ms2, imp, &now);
  CHECK(imp.state() == cs2::Ms2Importer::State::Failed && !imp.error().empty());
  CHECK(ms2.requests.size() == 2);  // un reintento
}

TEST(cs2_longitud_nunca_multiplo_de_8) {
  for (size_t n = 0; n < 40; n++) {
    const auto frames = cs2::configStream(makeId(CMD_CONFIG_DATA_REQUEST, false, 1), std::string(n, 'a'));
    CHECK(readBe32(frames[0].data) % 8 != 0);
  }
}
