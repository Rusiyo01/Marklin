#include <string.h>

#include <vector>

#include "marklin_can.h"
#include "marklin_controller.h"
#include "minitest.h"

using namespace marklin;

namespace {

struct FakeSink : CanSink {
  std::vector<CanFrame> sent;
  bool fail = false;
  bool send(const CanFrame& f) override {
    if (fail) return false;
    sent.push_back(f);
    return true;
  }
};

bool frameIs(const CanFrame& f, uint32_t id, std::vector<uint8_t> data) {
  if (f.id != id || f.dlc != data.size()) return false;
  return data.empty() || memcmp(f.data, data.data(), data.size()) == 0;
}

CanFrame frame(uint8_t cmd, bool resp, uint16_t hash, std::vector<uint8_t> data) {
  CanFrame f;
  f.id = makeId(cmd, resp, hash);
  f.dlc = static_cast<uint8_t>(data.size());
  if (!data.empty()) memcpy(f.data, data.data(), data.size());
  return f;
}

}  // namespace

TEST(hash_tiene_patron_cs2) {
  const uint32_t uids[] = {0, 1, 0x12345678, 0xFFFFFFFF, 0x4D4B0001, 0xDEADBEEF};
  for (uint32_t uid : uids) {
    const uint16_t h = hashFromUid(uid);
    CHECK_EQ((h >> 7) & 0x7, 0x6);  // bits 7..9 = 0b110
  }
  CHECK_EQ(hashFromUid(0), 0x0300);
  CHECK_EQ((sanitizeHash(0xFFFF) >> 7) & 0x7, 0x6);
  CHECK_EQ((sanitizeHash(0x0000) >> 7) & 0x7, 0x6);
}

TEST(tramas_iguales_a_can2udp) {
  // can2udp (GBert) envía estas tramas para arrancar una Gleisbox sin MS2.
  CHECK(frameIs(buildGleisboxStart(0x0301), 0x00360301, {0, 0, 0, 0, 0x11}));
  CHECK(frameIs(buildEnableProtocols(0x0301, 0x07), 0x00000301, {0, 0, 0, 0, 0x08, 0x07}));
  CHECK(frameIs(buildPing(0x4711), 0x00304711, {}));
}

TEST(tramas_de_loco) {
  CHECK(frameIs(buildSpeed(0x0301, locoUid(Protocol::MM2, 24), 500), 0x00080301,
                {0, 0, 0, 24, 0x01, 0xF4}));
  CHECK(frameIs(buildSpeed(0x0301, 24, 5000), 0x00080301, {0, 0, 0, 24, 0x03, 0xE8}));
  CHECK(frameIs(buildDirection(0x0301, 0xC003, DIR_REVERSE), 0x000A0301,
                {0, 0, 0xC0, 0x03, 2}));
  CHECK(frameIs(buildFunction(0x0301, 0x4005, 0, 1), 0x000C0301, {0, 0, 0x40, 0x05, 0, 1}));
  CHECK(frameIs(buildStop(0x0301), 0x00000301, {0, 0, 0, 0, 0}));
  CHECK(frameIs(buildGo(0x0301), 0x00000301, {0, 0, 0, 0, 1}));
  CHECK(frameIs(buildLocoEmergencyStop(0x0301, 24), 0x00000301, {0, 0, 0, 24, 3}));
  CHECK(frameIs(buildPowerQuery(0x0301), 0x00000301, {0, 0, 0, 0}));
}

TEST(tramas_de_accesorio) {
  CHECK_EQ(accessoryUid(Protocol::MM2, 1), 0x3000u);
  CHECK_EQ(accessoryUid(Protocol::DCC, 1), 0x3800u);
  CHECK(frameIs(buildAccessory(0x0301, 0x3004, ACC_STRAIGHT, 1), 0x00160301,
                {0, 0, 0x30, 0x04, 1, 1}));
  Protocol p;
  uint32_t a;
  CHECK(decodeAccessoryUid(0x3004, &p, &a));
  CHECK(p == Protocol::MM2);
  CHECK_EQ(a, 5u);
  CHECK(decodeAccessoryUid(0x3800, &p, &a));
  CHECK(p == Protocol::DCC);
  CHECK_EQ(a, 1u);
}

TEST(uids_de_loco) {
  CHECK_EQ(locoUid(Protocol::MM2, 24), 24u);
  CHECK_EQ(locoUid(Protocol::DCC, 3), 0xC003u);
  CHECK_EQ(locoUid(Protocol::MFX, 5), 0x4005u);
  Protocol p;
  uint32_t a;
  CHECK(decodeLocoUid(0xC003, &p, &a) && p == Protocol::DCC && a == 3);
  CHECK(decodeLocoUid(0x4005, &p, &a) && p == Protocol::MFX && a == 5);
  CHECK(decodeLocoUid(80, &p, &a) && p == Protocol::MM2 && a == 80);
  CHECK(!decodeLocoUid(0, &p, &a));
  CHECK(!decodeLocoUid(0x3000, &p, &a));  // accesorio, no loco
  char buf[24];
  formatLocoUid(0xC003, buf, sizeof(buf));
  CHECK_STR(buf, "dcc:3");
}

TEST(identificador) {
  CanFrame f = buildSpeed(0xABCD, 1, 1);
  CHECK_EQ(commandOf(f), CMD_LOCO_SPEED);
  CHECK(!isResponse(f));
  CHECK_EQ(hashOf(f), 0xABCD);
  f.id = makeId(CMD_PING, true, 0x1234);
  CHECK(isResponse(f));
  CHECK_EQ(commandOf(f), CMD_PING);
}

TEST(descripciones) {
  char buf[96];
  describeFrame(buildSpeed(0x0301, 0xC003, 400), buf, sizeof(buf));
  CHECK_STR(buf, "Loco dcc:3 velocidad 400");
  describeFrame(buildGo(0x0301), buf, sizeof(buf));
  CHECK(strstr(buf, "GO") != nullptr);
  describeFrame(frame(CMD_S88_EVENT, true, 0x1111, {0, 0, 0, 5, 0, 1, 0, 0}), buf, sizeof(buf));
  CHECK_STR(buf, "R S88 modulo 0 contacto 5: 0 -> 1");
  describeFrame(frame(CMD_PING, true, 0x1111, {0x12, 0x34, 0x56, 0x78, 3, 85, 0, 0x33}), buf,
                sizeof(buf));
  CHECK(strstr(buf, "MS2") != nullptr);
  describeFrame(frame(0x7E, false, 0x1111, {1, 2}), buf, sizeof(buf));
  CHECK_STR(buf, "Comando 0x7E dlc 2: 01 02");
}

TEST(controlador_velocidad_y_sentido) {
  FakeSink sink;
  Controller c(&sink);
  c.setUid(0x12345678);
  const uint16_t h = c.hash();
  CHECK(c.setSpeed(24, 300));
  CHECK_EQ(sink.sent.size(), 1u);
  CHECK(frameIs(sink.sent[0], makeId(CMD_LOCO_SPEED, false, h), {0, 0, 0, 24, 0x01, 0x2C}));
  CHECK_EQ(c.findLoco(24)->speed, 300);

  // Cambiar el sentido con la loco en marcha: primero velocidad 0.
  sink.sent.clear();
  CHECK(c.setDirection(24, DIR_REVERSE));
  CHECK_EQ(sink.sent.size(), 2u);
  CHECK_EQ(commandOf(sink.sent[0]), CMD_LOCO_SPEED);
  CHECK_EQ(readBe16(sink.sent[0].data + 4), 0);
  CHECK_EQ(commandOf(sink.sent[1]), CMD_LOCO_DIRECTION);
  CHECK_EQ(c.findLoco(24)->speed, 0);
  CHECK_EQ(c.findLoco(24)->direction, DIR_REVERSE);

  // Toggle con sentido conocido envía el sentido explícito.
  sink.sent.clear();
  c.setDirection(24, DIR_TOGGLE);
  CHECK_EQ(sink.sent.size(), 1u);
  CHECK_EQ(sink.sent[0].data[4], DIR_FORWARD);

  CHECK(c.setFunction(24, 3, true));
  CHECK_EQ(c.findLoco(24)->functions, 1u << 3);
  CHECK(c.setFunction(24, 3, false));
  CHECK_EQ(c.findLoco(24)->functions, 0u);
}

TEST(controlador_escucha_a_la_ms2) {
  FakeSink sink;
  Controller c(&sink);
  // La MS2 (otro hash) mueve la loco dcc:3.
  c.handleFrame(buildSpeed(0x5555, 0xC003, 640));
  CHECK_EQ(c.findLoco(0xC003)->speed, 640);
  c.handleFrame(buildDirection(0x5555, 0xC003, DIR_FORWARD));
  CHECK_EQ(c.findLoco(0xC003)->direction, DIR_FORWARD);
  // Cambio de sentido -> velocidad 0.
  c.handleFrame(buildSpeed(0x5555, 0xC003, 640));
  c.handleFrame(buildDirection(0x5555, 0xC003, DIR_REVERSE));
  CHECK_EQ(c.findLoco(0xC003)->speed, 0);
  c.handleFrame(buildFunction(0x5555, 0xC003, 0, 1));
  CHECK_EQ(c.findLoco(0xC003)->functions, 1u);

  CHECK(c.power() == PowerState::Unknown);
  // Respuesta de la Gleisbox a la consulta de estado.
  CanFrame st = buildGo(0x6666);
  st.id |= 0x10000;
  c.handleFrame(st);
  CHECK(c.power() == PowerState::On);
  c.handleFrame(buildGo(0x5555));
  CHECK(c.power() == PowerState::On);
  c.handleFrame(buildStop(0x5555));
  CHECK(c.power() == PowerState::Off);
  c.handleFrame(buildGo(0x5555));
  c.handleFrame(frame(CMD_SYSTEM, true, 0x5555, {0, 0, 0, 0, SYS_OVERLOAD, 1}));
  CHECK(c.overload());
  CHECK(c.power() == PowerState::Off);
  CHECK(sink.sent.empty());  // escuchar no envía nada
}

TEST(controlador_sensores_s88) {
  FakeSink sink;
  Controller c(&sink);
  CHECK_EQ(c.sensorState(5), -1);
  c.handleFrame(frame(CMD_S88_EVENT, true, 0x1111, {0, 0, 0, 5, 0, 1, 0, 10}));
  CHECK_EQ(c.sensorState(5), 1);
  CHECK_EQ(c.findSensor(5)->onCount, 1u);
  c.handleFrame(frame(CMD_S88_EVENT, true, 0x1111, {0, 0, 0, 5, 1, 0, 0, 10}));
  CHECK_EQ(c.sensorState(5), 0);
  CHECK_EQ(c.findSensor(5)->offCount, 1u);
  // Módulo distinto de 0: id = módulo * 1000 + contacto.
  c.handleFrame(frame(CMD_S88_EVENT, true, 0x1111, {0, 2, 0, 3, 0, 1, 0, 0}));
  CHECK_EQ(c.sensorState(2003), 1);
  // Sensores locales.
  c.setSensor(7, true);
  c.setSensor(7, true);  // repetido: no cuenta otro flanco
  CHECK_EQ(c.findSensor(7)->onCount, 1u);
}

TEST(controlador_accesorio_con_pulso) {
  FakeSink sink;
  Controller c(&sink);
  c.tick(1000);
  CHECK(c.switchAccessory(0x3004, ACC_ROUND, 200));
  CHECK_EQ(sink.sent.size(), 1u);
  CHECK_EQ(sink.sent[0].data[5], 1);
  c.tick(1100);
  CHECK_EQ(sink.sent.size(), 1u);
  c.tick(1200);
  CHECK_EQ(sink.sent.size(), 2u);
  CHECK_EQ(sink.sent[1].data[4], ACC_ROUND);
  CHECK_EQ(sink.sent[1].data[5], 0);
  c.tick(5000);
  CHECK_EQ(sink.sent.size(), 2u);
  CHECK_EQ(c.accessoryCount(), 1);
  CHECK_EQ(c.accessory(0).position, ACC_ROUND);
}

TEST(controlador_ping_detecta_ms2) {
  FakeSink sink;
  Controller c(&sink);
  CHECK(!c.ms2Present());
  c.handleFrame(frame(CMD_PING, true, 0x2222, {0x47, 0x11, 0x00, 0x01, 1, 39, 0x00, 0x10}));
  CHECK(!c.ms2Present());
  CHECK_EQ(c.deviceCount(), 1);
  c.handleFrame(frame(CMD_PING, true, 0x3333, {0x4D, 0x53, 0x00, 0x02, 3, 85, 0x00, 0x32}));
  CHECK(c.ms2Present());
  CHECK_EQ(c.deviceCount(), 2);
}

TEST(controlador_colision_de_hash) {
  FakeSink sink;
  Controller c(&sink);
  const uint16_t h = c.hash();
  c.handleFrame(buildSpeed(h, 1, 0));  // otro equipo usa nuestro hash
  CHECK(c.hash() != h);
  CHECK_EQ((c.hash() >> 7) & 0x7, 0x6);
  const uint16_t h2 = c.hash();
  CanFrame resp = buildSpeed(h2, 1, 0);
  resp.id |= 0x10000;  // las respuestas con nuestro hash son normales
  c.handleFrame(resp);
  CHECK_EQ(c.hash(), h2);
}

TEST(controlador_parar_todas) {
  FakeSink sink;
  Controller c(&sink);
  c.setSpeed(1, 100);
  c.setSpeed(2, 0);
  c.setSpeed(0xC003, 500);
  sink.sent.clear();
  CHECK_EQ(c.stopAllLocos(), 2);
  CHECK_EQ(sink.sent.size(), 2u);
  CHECK_EQ(c.findLoco(0xC003)->speed, 0);
}

TEST(controlador_errores_de_envio) {
  FakeSink sink;
  sink.fail = true;
  Controller c(&sink);
  CHECK(!c.setSpeed(1, 100));
  CHECK_EQ(c.txErrors(), 1u);
  CHECK(c.findLoco(1) == nullptr || c.findLoco(1)->speed == 0);
}
