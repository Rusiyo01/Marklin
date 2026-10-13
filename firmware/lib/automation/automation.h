// Motor de automatización: un pequeño lenguaje de scripts, línea a línea,
// para mover trenes de forma automática.
//
// Ejemplo:
//   loco tren mm2 24
//   power on
//   label inicio
//   dir tren fwd
//   ramp tren 50% 3s
//   wait sensor 2 on timeout 60s
//   speed tren 0
//   wait 10s
//   goto inicio
//
// Referencia completa del lenguaje en docs/scripts.md.
//
// Cada script se ejecuta en un "runner" cooperativo (sin hilos): tick()
// avanza hasta encontrar una espera y devuelve el control. Pueden correr
// varios scripts a la vez (un tren por script) y coordinarse con
// `lock`/`unlock` para reservar tramos de vía.
//
// Biblioteca portable: no depende de Arduino.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace automation {

// Lo que el motor necesita del resto del sistema.
class Host {
 public:
  virtual ~Host() = default;
  virtual uint32_t nowMs() = 0;
  virtual void setTrackPower(bool on) = 0;
  virtual void setSpeed(uint32_t loco, uint16_t speed) = 0;
  virtual uint16_t getSpeed(uint32_t loco) = 0;
  virtual void setDirection(uint32_t loco, uint8_t dir) = 0;  // 1 adelante, 2 atrás, 3 cambiar
  virtual void setFunction(uint32_t loco, uint8_t fn, bool on) = 0;
  virtual void emergencyStop(uint32_t loco) = 0;
  virtual void stopAllLocos() = 0;
  virtual void switchAccessory(uint32_t accUid, uint8_t position) = 0;
  // -1 desconocido, 0 libre, 1 activo.
  virtual int8_t sensorState(uint32_t id) = 0;
  // Número acumulado de flancos de subida (rising) o bajada del sensor.
  virtual uint32_t sensorEdges(uint32_t id, bool rising) = 0;
  // Número aleatorio en [lo, hi].
  virtual uint32_t random(uint32_t lo, uint32_t hi) = 0;
  virtual void log(const std::string& script, const std::string& msg) = 0;
  // Nombres globales definidos en la configuración (opcional).
  virtual bool resolveLoco(const std::string& name, uint32_t* uid) {
    (void)name; (void)uid;
    return false;
  }
  virtual bool resolveSensor(const std::string& name, uint32_t* id) {
    (void)name; (void)id;
    return false;
  }
  // Nombres de desvíos/señales (p.ej. los de la maqueta activa).
  virtual bool resolveAccessory(const std::string& name, uint32_t* uid) {
    (void)name; (void)uid;
    return false;
  }
};

enum class Op : uint8_t {
  Nop,
  Power,       // b = 1/0
  Speed,       // a = loco, b = velocidad
  Ramp,        // a = loco, b = velocidad final, c = duración ms
  Dir,         // a = loco, b = sentido
  Func,        // a = loco, b = función, c = 1/0
  EStop,       // a = loco
  Halt,        // todas las locos a 0
  Switch,      // a = uid accesorio, b = posición
  Wait,        // b = ms
  WaitRandom,  // b = min ms, c = max ms
  WaitSensor,  // a = sensor, b = estado, c = timeout ms (0 = sin), d = destino si timeout (-1 = error)
  Goto,        // b = destino
  IfSensor,    // a = sensor, b = estado, c = destino
  IfChance,    // b = porcentaje, c = destino
  IfLocked,    // text = bloqueo, c = destino (si lo tiene otro script)
  Repeat,      // b = veces (-1 = siempre), c = índice del `end`
  EndRepeat,   // c = índice del `repeat`
  Lock,        // text = nombre
  Unlock,      // text = nombre
  Log,         // text
  Exit,
};

struct Instr {
  Op op = Op::Nop;
  uint16_t line = 0;
  uint32_t a = 0;
  int32_t b = 0;
  int32_t c = 0;
  int32_t d = -1;
  std::string text;
};

struct Program {
  std::vector<Instr> code;
  std::string error;  // vacío si compiló bien
  int errorLine = 0;
  bool ok() const { return error.empty(); }
};

// Compila el texto de un script. `host` (opcional) resuelve nombres globales.
Program compile(const std::string& source, Host* host);

// Utilidades de análisis (expuestas para los tests y la consola).
bool parseDuration(const std::string& s, uint32_t* ms);
bool parseNumber(const std::string& s, uint32_t* out);
bool parseLocoRef(const std::string& s, uint32_t* uid);
bool parseAccessoryRef(const std::string& s, uint32_t* uid);

enum class RunState : uint8_t { Idle, Running, Waiting, Finished, Stopped, Error };
const char* runStateName(RunState s);

class Engine;

class Runner {
 public:
  RunState state() const { return state_; }
  bool active() const {
    return state_ == RunState::Running || state_ == RunState::Waiting;
  }
  const std::string& name() const { return name_; }
  const std::string& message() const { return message_; }
  int line() const;
  uint32_t startedMs() const { return startedMs_; }
  // Descripción de lo que está haciendo ahora mismo.
  std::string activity(uint32_t now) const;

 private:
  friend class Engine;

  enum class WaitKind : uint8_t { None, Time, Sensor, Lock, Ramp };
  struct RepeatFrame {
    int start;      // índice del `repeat`
    int remaining;  // -1 = infinito
  };

  void begin(const std::string& name, Program&& prog, uint32_t now);
  void tick(Engine& eng, Host& host, int slot);
  // Ejecuta una instrucción. Devuelve false si hay que esperar o terminó.
  bool step(Engine& eng, Host& host, int slot, int* actions);
  bool checkWait(Engine& eng, Host& host, int slot);
  void startWait(WaitKind kind, Host& host);
  void jump(int target);
  void touch(uint32_t loco);
  void fail(const std::string& msg);

  std::string name_;
  Program prog_;
  int pc_ = 0;
  RunState state_ = RunState::Idle;
  std::string message_;
  uint32_t startedMs_ = 0;

  WaitKind wait_ = WaitKind::None;
  int waitInstr_ = -1;     // instrucción que provocó la espera
  uint32_t waitStart_ = 0;
  uint32_t waitMs_ = 0;    // duración (esperas de tiempo)
  uint32_t edges0_ = 0;    // flancos del sensor al empezar a esperar

  uint32_t rampLoco_ = 0;
  int32_t rampFrom_ = 0;
  int32_t rampTo_ = 0;
  int32_t rampLast_ = -1;
  uint32_t rampLastSend_ = 0;

  std::vector<RepeatFrame> repeats_;
  std::vector<uint32_t> touched_;
};

class Engine {
 public:
  static constexpr int kMaxRunners = 4;
  static constexpr int kMaxStepsPerTick = 64;

  explicit Engine(Host* host) : host_(host) {}

  // Compila y arranca un script. Devuelve el slot o -1 (ver `error`).
  int start(const std::string& name, const std::string& source,
            std::string* error);
  // Detiene un script: pone a 0 las locos que haya manejado y libera sus
  // bloqueos.
  bool stop(int slot);
  bool stopByName(const std::string& name);
  void stopAll();
  void tick();

  bool isRunning(const std::string& name) const;
  const Runner& runner(int slot) const { return runners_[slot]; }
  int activeCount() const;

  // Ejecuta una orden inmediata de una línea (sin esperas ni saltos).
  // Devuelve "" si todo fue bien o el mensaje de error.
  std::string exec(const std::string& line);

  // Bloqueos con nombre (tramos de vía).
  bool tryLock(const std::string& name, int slot);
  void unlock(const std::string& name, int slot);
  void releaseLocks(int slot);
  // Dueño del bloqueo o -1.
  int lockOwner(const std::string& name) const;

 private:
  void abort(int slot, RunState finalState, const std::string& msg);

  Host* host_;
  Runner runners_[kMaxRunners];
  struct LockEntry {
    std::string name;
    int owner;
  };
  std::vector<LockEntry> locks_;
};

}  // namespace automation
