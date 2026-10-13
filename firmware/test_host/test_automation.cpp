#include <map>
#include <string>
#include <vector>

#include "automation.h"
#include "marklin_can.h"
#include "marklin_controller.h"
#include "minitest.h"
#include "../src/default_scripts.h"

using namespace automation;

namespace {

// Host simulado: registra las órdenes y permite controlar el tiempo.
struct FakeHost : Host {
  uint32_t now = 0;
  std::vector<std::string> calls;
  std::map<uint32_t, uint16_t> speeds;
  std::map<uint32_t, int8_t> sensors;
  std::map<uint32_t, uint32_t> rising, falling;
  std::vector<std::string> logs;
  uint32_t nextRandom = 0;  // 0 = devolver el mínimo

  uint32_t nowMs() override { return now; }
  void setTrackPower(bool on) override { calls.push_back(on ? "power on" : "power off"); }
  void setSpeed(uint32_t loco, uint16_t speed) override {
    speeds[loco] = speed;
    calls.push_back("speed " + std::to_string(loco) + " " + std::to_string(speed));
  }
  uint16_t getSpeed(uint32_t loco) override { return speeds[loco]; }
  void setDirection(uint32_t loco, uint8_t dir) override {
    speeds[loco] = 0;
    calls.push_back("dir " + std::to_string(loco) + " " + std::to_string(dir));
  }
  void setFunction(uint32_t loco, uint8_t fn, bool on) override {
    calls.push_back("func " + std::to_string(loco) + " " + std::to_string(fn) + (on ? " on" : " off"));
  }
  void emergencyStop(uint32_t loco) override { calls.push_back("estop " + std::to_string(loco)); }
  void stopAllLocos() override { calls.push_back("halt"); }
  void switchAccessory(uint32_t acc, uint8_t pos) override {
    calls.push_back("switch " + std::to_string(acc) + " " + std::to_string(pos));
  }
  int8_t sensorState(uint32_t id) override {
    auto it = sensors.find(id);
    return it == sensors.end() ? -1 : it->second;
  }
  uint32_t sensorEdges(uint32_t id, bool r) override { return r ? rising[id] : falling[id]; }
  uint32_t random(uint32_t lo, uint32_t hi) override {
    if (nextRandom == 0) return lo;
    return nextRandom < lo ? lo : (nextRandom > hi ? hi : nextRandom);
  }
  void log(const std::string& script, const std::string& msg) override {
    logs.push_back(script + ": " + msg);
  }
  bool resolveLoco(const std::string& name, uint32_t* uid) override {
    if (name == "cocodrilo") {
      *uid = 0xC00A;
      return true;
    }
    return false;
  }
  bool resolveSensor(const std::string& name, uint32_t* id) override {
    if (name == "anden") {
      *id = 42;
      return true;
    }
    return false;
  }
  bool resolveAccessory(const std::string& name, uint32_t* uid) override {
    if (name == "entrada") {
      *uid = 0x3000;
      return true;
    }
    return false;
  }

  void setSensor(uint32_t id, bool on) {
    const int8_t v = on ? 1 : 0;
    if (sensors.count(id) && sensors[id] == v) return;
    if (on)
      rising[id]++;
    else
      falling[id]++;
    sensors[id] = v;
  }
  // Avanza el tiempo en pasos de 10 ms ejecutando el motor.
  void run(Engine& e, uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 10) {
      now += 10;
      e.tick();
    }
  }
  bool called(const std::string& s) const {
    for (const auto& c : calls)
      if (c == s) return true;
    return false;
  }
};

std::string compileError(const std::string& src) {
  Program p = compile(src, nullptr);
  return p.error;
}

}  // namespace

TEST(duraciones) {
  uint32_t ms;
  CHECK(parseDuration("500ms", &ms) && ms == 500);
  CHECK(parseDuration("2s", &ms) && ms == 2000);
  CHECK(parseDuration("1.5s", &ms) && ms == 1500);
  CHECK(parseDuration("3", &ms) && ms == 3000);
  CHECK(parseDuration("2m", &ms) && ms == 120000);
  CHECK(parseDuration("2MIN", &ms) && ms == 120000);
  CHECK(!parseDuration("abc", &ms));
  CHECK(!parseDuration("5x", &ms));
  CHECK(!parseDuration("", &ms));
}

TEST(referencias_de_loco_y_accesorio) {
  uint32_t uid;
  CHECK(parseLocoRef("mm2:24", &uid) && uid == 24);
  CHECK(parseLocoRef("DCC:3", &uid) && uid == 0xC003);
  CHECK(parseLocoRef("mfx:5", &uid) && uid == 0x4005);
  CHECK(parseLocoRef("0x4007", &uid) && uid == 0x4007);
  CHECK(parseLocoRef("24", &uid) && uid == 24);
  CHECK(!parseLocoRef("mm2:0", &uid));
  CHECK(!parseLocoRef("mm2:300", &uid));
  CHECK(!parseLocoRef("xyz:3", &uid));
  CHECK(!parseLocoRef("0", &uid));
  CHECK(parseAccessoryRef("5", &uid) && uid == 0x3004);
  CHECK(parseAccessoryRef("mm:1", &uid) && uid == 0x3000);
  CHECK(parseAccessoryRef("dcc:12", &uid) && uid == 0x380B);
  CHECK(!parseAccessoryRef("0", &uid));
  CHECK(!parseAccessoryRef("mfx:3", &uid));
}

TEST(errores_de_compilacion) {
  CHECK(compileError("speed tren 100").find("loco desconocida") != std::string::npos);
  CHECK(compileError("loco tren mm2 24\nspeed tren 2000").find("velocidad") != std::string::npos);
  CHECK(compileError("goto nada").find("etiqueta no definida") != std::string::npos);
  CHECK(compileError("repeat 3\nwait 1s").find("falta 'end'") != std::string::npos);
  CHECK(compileError("end").find("sin 'repeat'") != std::string::npos);
  CHECK(compileError("bailar").find("orden desconocida") != std::string::npos);
  CHECK(compileError("label a\nlabel a").find("repetida") != std::string::npos);
  CHECK(compileError("wait sensor 1 on timeout 0s").find("timeout") != std::string::npos);
  Program p = compile("loco t mm2 24\n\nspeed t 50%\nbailar", nullptr);
  CHECK_EQ(p.errorLine, 4);
  CHECK(compile("# solo comentarios\n\n   // otro\n", nullptr).ok());
}

TEST(compila_ejemplos_incluidos) {
  for (int i = 0; i < kDefaultScriptCount; i++) {
    Program p = compile(kDefaultScripts[i].source, nullptr);
    if (!p.ok())
      printf("  %s: linea %d: %s\n", kDefaultScripts[i].name, p.errorLine, p.error.c_str());
    CHECK(p.ok());
  }
}

TEST(lanzadera_con_sensores) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  const char* src =
      "loco tren mm2 24\n"
      "power on\n"
      "label ida\n"
      "dir tren fwd\n"
      "speed tren 50%\n"
      "wait sensor 2 on\n"
      "speed tren 0\n"
      "wait 5s\n"
      "dir tren rev\n"
      "speed tren 500\n"
      "wait sensor 1 on\n"
      "speed tren 0\n"
      "exit\n";
  CHECK(e.start("lanzadera", src, &err) >= 0);
  CHECK_STR(err, "");
  h.run(e, 100);
  CHECK(h.called("power on"));
  CHECK(h.called("dir 24 1"));
  CHECK_EQ(h.speeds[24], 500);
  CHECK(e.runner(0).state() == RunState::Waiting);
  CHECK_EQ(e.runner(0).line(), 6);

  h.run(e, 10000);  // sin sensor no avanza
  CHECK_EQ(h.speeds[24], 500);
  h.setSensor(2, true);
  h.run(e, 20);
  CHECK_EQ(h.speeds[24], 0);
  h.run(e, 4000);
  CHECK(!h.called("dir 24 2"));
  h.run(e, 1100);
  CHECK(h.called("dir 24 2"));
  CHECK_EQ(h.speeds[24], 500);
  // Pulso corto en el sensor 1 (on y off antes del siguiente tick).
  h.setSensor(1, true);
  h.setSensor(1, false);
  h.run(e, 20);
  CHECK_EQ(h.speeds[24], 0);
  CHECK(e.runner(0).state() == RunState::Finished);
}

TEST(repeat_y_goto) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  const char* src =
      "repeat 3\n"
      "  func mm2:5 0 on\n"
      "  repeat 2\n"
      "    func mm2:5 1 on\n"
      "  end\n"
      "end\n"
      "repeat 0\n"
      "  func mm2:5 9 on\n"
      "end\n";
  CHECK(e.start("r", src, &err) >= 0);
  h.run(e, 200);
  int f0 = 0, f1 = 0, f9 = 0;
  for (const auto& c : h.calls) {
    if (c == "func 5 0 on") f0++;
    if (c == "func 5 1 on") f1++;
    if (c == "func 5 9 on") f9++;
  }
  CHECK_EQ(f0, 3);
  CHECK_EQ(f1, 6);
  CHECK_EQ(f9, 0);
  CHECK(e.runner(0).state() == RunState::Finished);

  // goto fuera de un repeat infinito.
  FakeHost h2;
  Engine e2(&h2);
  const char* src2 =
      "repeat\n"
      "  func 7 0 on\n"
      "  if sensor 3 on goto fuera\n"
      "  wait 1s\n"
      "end\n"
      "label fuera\n"
      "func 7 0 off\n";
  CHECK(e2.start("g", src2, &err) >= 0);
  h2.run(e2, 3500);
  CHECK(!h2.called("func 7 0 off"));
  h2.setSensor(3, true);
  h2.run(e2, 1100);
  CHECK(h2.called("func 7 0 off"));
  CHECK(e2.runner(0).state() == RunState::Finished);
}

TEST(timeout_de_sensor) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  // Con 'else': salta a la etiqueta.
  CHECK(e.start("a", "speed 3 200\nwait sensor 9 on timeout 2s else fallo\nexit\nlabel fallo\nlog sin sensor\nspeed 3 0\n", &err) >= 0);
  h.run(e, 2100);
  CHECK_EQ(h.speeds[3], 0);
  CHECK(e.runner(0).state() == RunState::Finished);

  // Sin 'else': error y parada de seguridad.
  FakeHost h2;
  Engine e2(&h2);
  CHECK(e2.start("b", "speed 4 300\nwait sensor 9 on timeout 1s\nspeed 4 999\n", &err) >= 0);
  h2.run(e2, 500);
  CHECK_EQ(h2.speeds[4], 300);
  h2.run(e2, 700);
  CHECK(e2.runner(0).state() == RunState::Error);
  CHECK_EQ(h2.speeds[4], 0);
  CHECK(e2.runner(0).message().find("timeout") != std::string::npos);
}

TEST(rampa) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  CHECK(e.start("r", "ramp dcc:3 1000 1s\nlog fin\n", &err) >= 0);
  h.run(e, 500);
  const uint16_t mid = h.speeds[0xC003];
  CHECK(mid >= 400 && mid <= 600);
  CHECK(e.runner(0).state() == RunState::Waiting);
  h.run(e, 600);
  CHECK_EQ(h.speeds[0xC003], 1000);
  CHECK(e.runner(0).state() == RunState::Finished);
  // No más de ~1 orden cada 100 ms.
  int n = 0;
  for (const auto& c : h.calls)
    if (c.rfind("speed 49155", 0) == 0) n++;
  CHECK(n >= 5 && n <= 12);

  // Rampa de frenado desde la velocidad actual.
  h.calls.clear();
  CHECK(e.start("r2", "ramp dcc:3 0 500ms\n", &err) >= 0);
  h.run(e, 600);
  CHECK_EQ(h.speeds[0xC003], 0);
}

TEST(bloqueos_entre_dos_trenes) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  const char* a =
      "lock tramo\n"
      "speed 1 400\n"
      "wait 3s\n"
      "speed 1 0\n"
      "unlock tramo\n";
  const char* b =
      "wait 100ms\n"
      "lock tramo\n"
      "speed 2 400\n"
      "wait 1s\n";
  CHECK(e.start("a", a, &err) >= 0);
  CHECK(e.start("b", b, &err) >= 0);
  h.run(e, 1000);
  CHECK_EQ(h.speeds[1], 400);
  CHECK_EQ(h.speeds[2], 0);
  CHECK(e.runner(1).activity(h.now).find("tramo") != std::string::npos);
  CHECK_EQ(e.lockOwner("tramo"), 0);
  h.run(e, 2100);
  CHECK_EQ(h.speeds[1], 0);
  CHECK_EQ(h.speeds[2], 400);
  CHECK_EQ(e.lockOwner("tramo"), 1);
  h.run(e, 1100);
  // Al terminar el script se liberan sus bloqueos.
  CHECK_EQ(e.lockOwner("tramo"), -1);
}

TEST(detener_para_las_locos) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  int slot = e.start("x", "loco t dcc 7\nspeed t 600\nfunc t 0 on\nwait 1m\n", &err);
  CHECK(slot >= 0);
  h.run(e, 100);
  CHECK_EQ(h.speeds[0xC007], 600);
  CHECK(e.stop(slot));
  CHECK_EQ(h.speeds[0xC007], 0);
  CHECK(e.runner(slot).state() == RunState::Stopped);
  CHECK(!e.stop(slot));
}

TEST(no_dos_veces_el_mismo_script) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  CHECK(e.start("x", "wait 10s", &err) >= 0);
  CHECK_EQ(e.start("x", "wait 10s", &err), -1);
  CHECK(err.find("ya esta en marcha") != std::string::npos);
  CHECK(e.start("y", "wait 10s", &err) >= 0);
  CHECK(e.start("z", "wait 10s", &err) >= 0);
  CHECK(e.start("w", "wait 10s", &err) >= 0);
  CHECK_EQ(e.start("v", "wait 10s", &err), -1);
  CHECK(err.find("huecos") != std::string::npos);
  CHECK_EQ(e.activeCount(), 4);
  e.stopAll();
  CHECK_EQ(e.activeCount(), 0);
}

TEST(bucle_sin_esperas_no_bloquea) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  CHECK(e.start("loop", "label a\nfunc 1 0 on\ngoto a\n", &err) >= 0);
  h.now += 10;
  e.tick();  // debe volver (pausa implícita)
  CHECK(h.calls.size() <= 16);
  CHECK(e.runner(0).state() == RunState::Waiting);
  e.stopAll();
}

TEST(nombres_globales_y_alias) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  CHECK(e.start("g", "sensor puerta 8\nspeed cocodrilo 100\nwait sensor anden on\nwait sensor puerta off\nlog \"Hola, Mundo # no es comentario\"  # esto si\n", &err) >= 0);
  h.run(e, 50);
  CHECK_EQ(h.speeds[0xC00A], 100);
  h.setSensor(42, true);
  h.setSensor(8, true);
  h.setSensor(8, false);
  h.run(e, 50);
  CHECK(e.runner(0).state() == RunState::Finished);
  bool found = false;
  for (const auto& l : h.logs)
    if (l == "g: Hola, Mundo # no es comentario") found = true;
  CHECK(found);
}

TEST(desvios_con_nombre_de_la_maqueta) {
  FakeHost h;
  Engine e(&h);
  CHECK_STR(e.exec("switch entrada round"), "");
  CHECK(h.called("switch 12288 0"));
  CHECK(e.exec("switch salida round").find("desvio desconocido") != std::string::npos);
  // Un número sigue siendo una dirección MM aunque haya nombres.
  CHECK_STR(e.exec("switch 2 straight"), "");
  CHECK(h.called("switch 12289 1"));
}

TEST(espera_aleatoria_y_azar) {
  FakeHost h;
  Engine e(&h);
  std::string err;
  h.nextRandom = 3000;
  CHECK(e.start("r", "wait random 1s 5s\nif chance 50 goto b\nfunc 1 1 on\nexit\nlabel b\nfunc 1 2 on\n", &err) >= 0);
  h.run(e, 2900);
  CHECK(h.calls.empty());
  h.nextRandom = 80;  // 80 > 50: no salta
  h.run(e, 200);
  CHECK(h.called("func 1 1 on"));
}

TEST(orden_inmediata) {
  FakeHost h;
  Engine e(&h);
  CHECK_STR(e.exec("speed dcc:3 250"), "");
  CHECK_EQ(h.speeds[0xC003], 250);
  CHECK_STR(e.exec("switch 5 straight"), "");
  CHECK(h.called("switch 12292 1"));
  CHECK_STR(e.exec("power off"), "");
  CHECK(e.exec("wait 1s").find("no permitida") != std::string::npos);
  CHECK(e.exec("speed nadie 3").find("desconocida") != std::string::npos);
}

TEST(integracion_con_controlador) {
  // El motor manejando el controlador real con un bus simulado.
  struct Sink : marklin::CanSink {
    std::vector<marklin::CanFrame> sent;
    bool send(const marklin::CanFrame& f) override {
      sent.push_back(f);
      return true;
    }
  } sink;
  marklin::Controller ctrl(&sink);
  struct CtrlHost : FakeHost {
    marklin::Controller* c = nullptr;
    void setSpeed(uint32_t l, uint16_t s) override { c->setSpeed(l, s); }
    uint16_t getSpeed(uint32_t l) override {
      const marklin::LocoState* st = c->findLoco(l);
      return st ? st->speed : 0;
    }
    void setDirection(uint32_t l, uint8_t d) override {
      c->setDirection(l, static_cast<marklin::Direction>(d));
    }
    int8_t sensorState(uint32_t id) override { return c->sensorState(id); }
    uint32_t sensorEdges(uint32_t id, bool r) override {
      const marklin::SensorState* s = c->findSensor(id);
      return s ? (r ? s->onCount : s->offCount) : 0;
    }
  } h;
  h.c = &ctrl;
  Engine e(&h);
  std::string err;
  CHECK(e.start("t", "dir mm2:24 fwd\nspeed mm2:24 700\nwait sensor 3 on\nspeed mm2:24 0\n", &err) >= 0);
  h.run(e, 50);
  CHECK_EQ(ctrl.findLoco(24)->speed, 700);
  // Llega un evento S88 del bus (p.ej. de un Link S88).
  marklin::CanFrame ev;
  ev.id = marklin::makeId(marklin::CMD_S88_EVENT, true, 0x1234);
  ev.dlc = 8;
  ev.data[3] = 3;
  ev.data[5] = 1;
  ctrl.handleFrame(ev);
  h.run(e, 20);
  CHECK_EQ(ctrl.findLoco(24)->speed, 0);
  CHECK(e.runner(0).state() == RunState::Finished);
}
