#include "automation.h"

#include <stdio.h>
#include <stdlib.h>

#include "marklin_can.h"

namespace automation {

// ---- Utilidades de texto -----------------------------------------------------

static std::string lower(const std::string& s) {
  std::string r = s;
  for (auto& ch : r)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return r;
}

// Quita comentarios (# o //) que no estén dentro de comillas.
static std::string stripComment(const std::string& line) {
  bool quoted = false;
  for (size_t i = 0; i < line.size(); i++) {
    const char ch = line[i];
    if (ch == '"') quoted = !quoted;
    if (quoted) continue;
    if (ch == '#') return line.substr(0, i);
    if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') return line.substr(0, i);
  }
  return line;
}

static std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char ch : s) {
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == ',') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur += ch;
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

static std::string toStr(long v) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", v);
  return buf;
}

bool parseNumber(const std::string& s, uint32_t* out) {
  if (s.empty()) return false;
  const char* p = s.c_str();
  char* end = nullptr;
  unsigned long v;
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    v = strtoul(p + 2, &end, 16);
  else
    v = strtoul(p, &end, 10);
  if (!end || *end != 0 || s[0] == '-') return false;
  *out = static_cast<uint32_t>(v);
  return true;
}

bool parseDuration(const std::string& raw, uint32_t* ms) {
  const std::string s = lower(raw);
  size_t i = 0;
  double value = 0;
  bool digits = false;
  while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
    value = value * 10 + (s[i] - '0');
    i++;
    digits = true;
  }
  if (i < s.size() && s[i] == '.') {
    i++;
    double scale = 0.1;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
      value += (s[i] - '0') * scale;
      scale /= 10;
      i++;
      digits = true;
    }
  }
  if (!digits) return false;
  const std::string unit = s.substr(i);
  double factor;
  if (unit.empty() || unit == "s" || unit == "seg") factor = 1000;
  else if (unit == "ms") factor = 1;
  else if (unit == "m" || unit == "min") factor = 60000;
  else if (unit == "h") factor = 3600000;
  else return false;
  const double r = value * factor;
  if (r > 4.0e9) return false;
  *ms = static_cast<uint32_t>(r + 0.5);
  return true;
}

static bool parseProtoAddress(const std::string& s, marklin::Protocol* proto,
                              uint32_t* addr, bool* hasProto) {
  const size_t colon = s.find(':');
  *hasProto = colon != std::string::npos;
  if (!*hasProto) {
    *proto = marklin::Protocol::RAW;
    return parseNumber(s, addr);
  }
  const std::string p = s.substr(0, colon);
  if (p == "mm" || p == "mm2" || p == "mm1") *proto = marklin::Protocol::MM2;
  else if (p == "mfx") *proto = marklin::Protocol::MFX;
  else if (p == "dcc") *proto = marklin::Protocol::DCC;
  else if (p == "sx1" || p == "sx") *proto = marklin::Protocol::SX1;
  else if (p == "uid") *proto = marklin::Protocol::RAW;
  else return false;
  return parseNumber(s.substr(colon + 1), addr);
}

bool parseLocoRef(const std::string& raw, uint32_t* uid) {
  marklin::Protocol proto;
  uint32_t addr;
  bool hasProto;
  if (!parseProtoAddress(lower(raw), &proto, &addr, &hasProto)) return false;
  if (hasProto && proto != marklin::Protocol::RAW) {
    if (addr == 0) return false;
    if (proto == marklin::Protocol::MM2 && addr > 255) return false;
    if (proto == marklin::Protocol::DCC && addr > 10239) return false;
    if (proto == marklin::Protocol::MFX && addr > 0x3FFF) return false;
  }
  const uint32_t u = marklin::locoUid(proto, addr);
  if (!marklin::isLocoUid(u)) return false;
  *uid = u;
  return true;
}

bool parseAccessoryRef(const std::string& raw, uint32_t* uid) {
  marklin::Protocol proto;
  uint32_t addr;
  bool hasProto;
  if (!parseProtoAddress(lower(raw), &proto, &addr, &hasProto)) return false;
  if (proto == marklin::Protocol::RAW && hasProto) {
    if (!marklin::isAccessoryUid(addr)) return false;
    *uid = addr;
    return true;
  }
  if (proto == marklin::Protocol::RAW) proto = marklin::Protocol::MM2;
  if (proto != marklin::Protocol::MM2 && proto != marklin::Protocol::DCC) return false;
  if (addr == 0) return false;
  if (proto == marklin::Protocol::MM2 && addr > 1024) return false;
  if (proto == marklin::Protocol::DCC && addr > 2048) return false;
  *uid = marklin::accessoryUid(proto, addr);
  return true;
}

static bool parseOnOff(const std::string& s, int32_t* v) {
  if (s == "on" || s == "1" || s == "si" || s == "true" || s == "ocupado" || s == "activo") {
    *v = 1;
    return true;
  }
  if (s == "off" || s == "0" || s == "no" || s == "false" || s == "libre" || s == "inactivo") {
    *v = 0;
    return true;
  }
  return false;
}

static bool parseDir(const std::string& s, int32_t* v) {
  if (s == "fwd" || s == "forward" || s == "adelante" || s == "avance" || s == ">") {
    *v = marklin::DIR_FORWARD;
  } else if (s == "rev" || s == "reverse" || s == "back" || s == "atras" || s == "retroceso" || s == "<") {
    *v = marklin::DIR_REVERSE;
  } else if (s == "toggle" || s == "cambiar" || s == "invertir") {
    *v = marklin::DIR_TOGGLE;
  } else {
    return false;
  }
  return true;
}

static bool parseSpeed(const std::string& s, int32_t* v) {
  uint32_t n;
  if (!s.empty() && s.back() == '%') {
    if (!parseNumber(s.substr(0, s.size() - 1), &n) || n > 100) return false;
    *v = static_cast<int32_t>(n * 10);
    return true;
  }
  if (!parseNumber(s, &n) || n > marklin::kMaxSpeed) return false;
  *v = static_cast<int32_t>(n);
  return true;
}

static bool parseFunction(const std::string& s, int32_t* v) {
  uint32_t n;
  const std::string t = (!s.empty() && s[0] == 'f') ? s.substr(1) : s;
  if (!parseNumber(t, &n) || n > 31) return false;
  *v = static_cast<int32_t>(n);
  return true;
}

static bool parsePosition(const std::string& s, int32_t* v) {
  if (s == "straight" || s == "recto" || s == "green" || s == "verde" || s == "1") {
    *v = marklin::ACC_STRAIGHT;
  } else if (s == "round" || s == "diverge" || s == "desviado" || s == "curve" ||
             s == "curva" || s == "red" || s == "rojo" || s == "0") {
    *v = marklin::ACC_ROUND;
  } else if (s == "yellow" || s == "amarillo" || s == "2") {
    *v = marklin::ACC_YELLOW;
  } else if (s == "white" || s == "blanco" || s == "3") {
    *v = marklin::ACC_WHITE;
  } else {
    return false;
  }
  return true;
}

static bool isIdentifier(const std::string& s) {
  if (s.empty()) return false;
  const char c0 = s[0];
  if (!((c0 >= 'a' && c0 <= 'z') || c0 == '_')) return false;
  for (char ch : s)
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-'))
      return false;
  return true;
}

// ---- Compilador ------------------------------------------------------------------

Program compile(const std::string& source, Host* host) {
  Program p;
  struct Alias {
    std::string name;
    uint32_t value;
  };
  struct Label {
    std::string name;
    int index;
  };
  struct Fixup {
    int instr;
    char field;  // 'b', 'c' o 'd'
    std::string label;
    int line;
  };
  std::vector<Alias> locos, sensors;
  std::vector<Label> labels;
  std::vector<Fixup> fixups;
  std::vector<int> repeatStack;

  int lineNo = 0;
  auto fail = [&](const std::string& msg) {
    if (p.error.empty()) {
      p.error = msg;
      p.errorLine = lineNo;
    }
  };
  auto findAlias = [](const std::vector<Alias>& v, const std::string& n, uint32_t* out) {
    for (const auto& a : v)
      if (a.name == n) {
        *out = a.value;
        return true;
      }
    return false;
  };
  auto loco = [&](const std::string& tok, uint32_t* uid) {
    if (findAlias(locos, tok, uid)) return true;
    if (host && host->resolveLoco(tok, uid)) return true;
    if (parseLocoRef(tok, uid)) return true;
    fail("loco desconocida: '" + tok + "' (usa un alias definido con 'loco' o p.ej. mm2:24, dcc:3, mfx:5)");
    return false;
  };
  auto sensor = [&](const std::string& tok, uint32_t* id) {
    if (findAlias(sensors, tok, id)) return true;
    if (host && host->resolveSensor(tok, id)) return true;
    if (parseNumber(tok, id)) return true;
    fail("sensor desconocido: '" + tok + "'");
    return false;
  };
  auto duration = [&](const std::string& tok, uint32_t* ms) {
    if (parseDuration(tok, ms)) return true;
    fail("duracion no valida: '" + tok + "' (ej. 500ms, 2s, 1.5m)");
    return false;
  };
  auto addFixup = [&](char field, const std::string& label) {
    fixups.push_back({static_cast<int>(p.code.size()), field, label, lineNo});
  };

  size_t pos = 0;
  while (pos <= source.size() && p.ok()) {
    size_t nl = source.find('\n', pos);
    if (nl == std::string::npos) nl = source.size();
    const std::string rawLine = source.substr(pos, nl - pos);
    pos = nl + 1;
    lineNo++;

    const std::string body = stripComment(rawLine);
    const std::vector<std::string> orig = split(body);
    if (orig.empty()) continue;
    std::vector<std::string> t;
    for (const auto& s : orig) t.push_back(lower(s));
    const std::string& kw = t[0];
    const size_t n = t.size() - 1;  // número de argumentos

    Instr in;
    in.line = static_cast<uint16_t>(lineNo);
    bool emit = true;
    auto need = [&](size_t count, const char* usage) {
      if (n != count) {
        fail(std::string("uso: ") + usage);
        return false;
      }
      return true;
    };

    if (kw == "loco") {
      emit = false;
      if (n != 2 && n != 3) {
        fail("uso: loco <nombre> <protocolo> <direccion>  (ej. loco tren1 mm2 24)");
      } else if (!isIdentifier(t[1])) {
        fail("nombre de loco no valido: '" + t[1] + "'");
      } else {
        const std::string ref = n == 3 ? t[2] + ":" + t[3] : t[2];
        uint32_t uid;
        if (parseLocoRef(ref, &uid))
          locos.push_back({t[1], uid});
        else
          fail("direccion de loco no valida: '" + ref + "'");
      }
    } else if (kw == "sensor") {
      emit = false;
      uint32_t id;
      if (!need(2, "sensor <nombre> <numero>")) {
      } else if (!isIdentifier(t[1])) {
        fail("nombre de sensor no valido: '" + t[1] + "'");
      } else if (!parseNumber(t[2], &id)) {
        fail("numero de sensor no valido: '" + t[2] + "'");
      } else {
        sensors.push_back({t[1], id});
      }
    } else if (kw == "power" || kw == "corriente") {
      in.op = Op::Power;
      if (need(1, "power on|off") && !parseOnOff(t[1], &in.b)) fail("se esperaba on u off");
    } else if (kw == "go") {
      in.op = Op::Power;
      in.b = 1;
      need(0, "go");
    } else if (kw == "speed" || kw == "velocidad") {
      in.op = Op::Speed;
      if (need(2, "speed <loco> <0-1000 | 0-100%>") && loco(t[1], &in.a) &&
          !parseSpeed(t[2], &in.b))
        fail("velocidad no valida: '" + t[2] + "' (0-1000 o 0-100%)");
    } else if (kw == "ramp" || kw == "rampa") {
      in.op = Op::Ramp;
      uint32_t ms;
      if (need(3, "ramp <loco> <velocidad> <duracion>") && loco(t[1], &in.a)) {
        if (!parseSpeed(t[2], &in.b))
          fail("velocidad no valida: '" + t[2] + "'");
        else if (duration(t[3], &ms))
          in.c = static_cast<int32_t>(ms);
      }
    } else if (kw == "dir" || kw == "sentido") {
      in.op = Op::Dir;
      if (need(2, "dir <loco> fwd|rev|toggle") && loco(t[1], &in.a) && !parseDir(t[2], &in.b))
        fail("sentido no valido: '" + t[2] + "' (fwd, rev o toggle)");
    } else if (kw == "func" || kw == "fn" || kw == "funcion") {
      in.op = Op::Func;
      if (need(3, "func <loco> <0-31> on|off") && loco(t[1], &in.a)) {
        if (!parseFunction(t[2], &in.b))
          fail("funcion no valida: '" + t[2] + "' (0-31 o f0-f31)");
        else if (!parseOnOff(t[3], &in.c))
          fail("se esperaba on u off");
      }
    } else if (kw == "estop" || kw == "emergencia") {
      in.op = Op::EStop;
      if (need(1, "estop <loco>")) loco(t[1], &in.a);
    } else if (kw == "halt" || kw == "parar_todo") {
      in.op = Op::Halt;
      need(0, "halt");
    } else if (kw == "switch" || kw == "turnout" || kw == "signal" || kw == "desvio" ||
               kw == "senal") {
      in.op = Op::Switch;
      if (need(2, "switch <direccion> straight|round|yellow|white")) {
        if (!parseAccessoryRef(t[1], &in.a) && !(host && host->resolveAccessory(t[1], &in.a)))
          fail("desvio desconocido: '" + t[1] + "' (ej. 5, mm:5, dcc:12 o un nombre de la maqueta)");
        else if (!parsePosition(t[2], &in.b))
          fail("posicion no valida: '" + t[2] + "' (straight/recto, round/desviado, yellow, white)");
      }
    } else if (kw == "wait" || kw == "esperar") {
      if (n >= 1 && t[1] == "random") {
        in.op = Op::WaitRandom;
        uint32_t lo, hi;
        if (need(3, "wait random <min> <max>") && duration(t[2], &lo) && duration(t[3], &hi)) {
          if (hi < lo) fail("en 'wait random' el maximo debe ser >= minimo");
          in.b = static_cast<int32_t>(lo);
          in.c = static_cast<int32_t>(hi);
        }
      } else if (n >= 1 && t[1] == "sensor") {
        in.op = Op::WaitSensor;
        const char* usage = "wait sensor <sensor> on|off [timeout <duracion> [else <etiqueta>]]";
        if (n != 3 && n != 5 && n != 7) {
          fail(std::string("uso: ") + usage);
        } else if (sensor(t[2], &in.a)) {
          if (!parseOnOff(t[3], &in.b)) {
            fail("se esperaba on u off");
          } else if (n >= 5) {
            uint32_t ms;
            if (t[4] != "timeout") {
              fail(std::string("uso: ") + usage);
            } else if (duration(t[5], &ms)) {
              if (ms == 0) fail("el timeout debe ser mayor que 0");
              in.c = static_cast<int32_t>(ms);
              if (n == 7) {
                if (t[6] != "else")
                  fail(std::string("uso: ") + usage);
                else
                  addFixup('d', t[7]);
              }
            }
          }
        }
      } else {
        in.op = Op::Wait;
        uint32_t ms;
        if (need(1, "wait <duracion> | wait sensor ... | wait random ...") && duration(t[1], &ms))
          in.b = static_cast<int32_t>(ms);
      }
    } else if (kw == "label" || kw == "etiqueta") {
      emit = false;
      if (need(1, "label <nombre>")) {
        if (!isIdentifier(t[1])) {
          fail("nombre de etiqueta no valido: '" + t[1] + "'");
        } else {
          for (const auto& l : labels)
            if (l.name == t[1]) fail("etiqueta repetida: '" + t[1] + "'");
          labels.push_back({t[1], static_cast<int>(p.code.size())});
        }
      }
    } else if (kw == "goto" || kw == "ir") {
      in.op = Op::Goto;
      if (need(1, "goto <etiqueta>")) addFixup('b', t[1]);
    } else if (kw == "if" || kw == "si") {
      if (n >= 1 && t[1] == "sensor") {
        in.op = Op::IfSensor;
        if (need(5, "if sensor <sensor> on|off goto <etiqueta>") && sensor(t[2], &in.a)) {
          if (!parseOnOff(t[3], &in.b))
            fail("se esperaba on u off");
          else if (t[4] != "goto")
            fail("uso: if sensor <sensor> on|off goto <etiqueta>");
          else
            addFixup('c', t[5]);
        }
      } else if (n >= 1 && t[1] == "chance") {
        in.op = Op::IfChance;
        uint32_t pct;
        if (need(4, "if chance <1-100> goto <etiqueta>")) {
          if (!parseNumber(t[2], &pct) || pct > 100)
            fail("porcentaje no valido: '" + t[2] + "'");
          else if (t[3] != "goto")
            fail("uso: if chance <1-100> goto <etiqueta>");
          else {
            in.b = static_cast<int32_t>(pct);
            addFixup('c', t[4]);
          }
        }
      } else if (n >= 1 && t[1] == "locked") {
        in.op = Op::IfLocked;
        if (need(4, "if locked <bloqueo> goto <etiqueta>")) {
          if (t[3] != "goto") {
            fail("uso: if locked <bloqueo> goto <etiqueta>");
          } else {
            in.text = t[2];
            addFixup('c', t[4]);
          }
        }
      } else {
        fail("uso: if sensor <s> on|off goto <etiqueta> | if chance <1-100> goto <etiqueta> | "
             "if locked <bloqueo> goto <etiqueta>");
      }
    } else if (kw == "repeat" || kw == "repetir") {
      in.op = Op::Repeat;
      if (n == 0 || (n == 1 && (t[1] == "forever" || t[1] == "siempre"))) {
        in.b = -1;
      } else if (n == 1) {
        uint32_t count;
        if (parseNumber(t[1], &count) && count < 1000000)
          in.b = static_cast<int32_t>(count);
        else
          fail("numero de repeticiones no valido: '" + t[1] + "'");
      } else {
        fail("uso: repeat [veces]");
      }
      repeatStack.push_back(static_cast<int>(p.code.size()));
    } else if (kw == "end" || kw == "fin") {
      in.op = Op::EndRepeat;
      if (!need(0, "end")) {
      } else if (repeatStack.empty()) {
        fail("'end' sin 'repeat'");
      } else {
        const int start = repeatStack.back();
        repeatStack.pop_back();
        in.c = start;
        p.code[start].c = static_cast<int32_t>(p.code.size());
      }
    } else if (kw == "lock" || kw == "reservar") {
      in.op = Op::Lock;
      if (need(1, "lock <nombre>")) in.text = t[1];
    } else if (kw == "unlock" || kw == "liberar") {
      in.op = Op::Unlock;
      if (need(1, "unlock <nombre>")) in.text = t[1];
    } else if (kw == "log" || kw == "print") {
      in.op = Op::Log;
      // Texto original (con mayúsculas), sin la palabra clave ni comillas.
      std::string text = body;
      size_t s = text.find_first_not_of(" \t");
      s = text.find_first_of(" \t", s);
      text = s == std::string::npos ? "" : text.substr(s);
      const size_t a = text.find_first_not_of(" \t");
      const size_t b = text.find_last_not_of(" \t\r");
      text = a == std::string::npos ? "" : text.substr(a, b - a + 1);
      if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
        text = text.substr(1, text.size() - 2);
      in.text = text;
    } else if (kw == "exit" || kw == "salir") {
      in.op = Op::Exit;
      need(0, "exit");
    } else {
      fail("orden desconocida: '" + orig[0] + "'");
    }

    if (emit && p.ok()) p.code.push_back(in);
  }

  if (p.ok() && !repeatStack.empty()) {
    lineNo = p.code[repeatStack.back()].line;
    fail("falta 'end' para este 'repeat'");
  }
  if (p.ok()) {
    for (const auto& f : fixups) {
      int target = -1;
      for (const auto& l : labels)
        if (l.name == f.label) target = l.index;
      if (target < 0) {
        lineNo = f.line;
        fail("etiqueta no definida: '" + f.label + "'");
        break;
      }
      Instr& in = p.code[f.instr];
      if (f.field == 'b') in.b = target;
      else if (f.field == 'c') in.c = target;
      else in.d = target;
    }
  }
  if (!p.ok()) p.code.clear();
  return p;
}

// ---- Runner -------------------------------------------------------------------------

const char* runStateName(RunState s) {
  switch (s) {
    case RunState::Idle: return "libre";
    case RunState::Running: return "ejecutando";
    case RunState::Waiting: return "esperando";
    case RunState::Finished: return "terminado";
    case RunState::Stopped: return "detenido";
    case RunState::Error: return "error";
  }
  return "?";
}

void Runner::begin(const std::string& name, Program&& prog, uint32_t now) {
  name_ = name;
  prog_ = std::move(prog);
  pc_ = 0;
  state_ = RunState::Running;
  message_.clear();
  startedMs_ = now;
  wait_ = WaitKind::None;
  waitInstr_ = -1;
  repeats_.clear();
  touched_.clear();
}

int Runner::line() const {
  const int idx = (wait_ != WaitKind::None && waitInstr_ >= 0) ? waitInstr_ : pc_;
  if (!active() || idx < 0 || idx >= static_cast<int>(prog_.code.size())) return 0;
  return prog_.code[idx].line;
}

std::string Runner::activity(uint32_t now) const {
  if (!active()) return message_.empty() ? runStateName(state_) : message_;
  if (wait_ == WaitKind::None) return "ejecutando";
  const Instr* in = (waitInstr_ >= 0 && waitInstr_ < static_cast<int>(prog_.code.size()))
                        ? &prog_.code[waitInstr_]
                        : nullptr;
  const uint32_t elapsed = now - waitStart_;
  char buf[96];
  switch (wait_) {
    case WaitKind::Time:
      snprintf(buf, sizeof(buf), "esperando %.1f s de %.1f s", elapsed / 1000.0, waitMs_ / 1000.0);
      return buf;
    case WaitKind::Sensor:
      if (!in) return "esperando sensor";
      if (in->c > 0)
        snprintf(buf, sizeof(buf), "esperando sensor %lu %s (%lu s / timeout %lu s)",
                 static_cast<unsigned long>(in->a), in->b ? "on" : "off",
                 static_cast<unsigned long>(elapsed / 1000), static_cast<unsigned long>(in->c / 1000));
      else
        snprintf(buf, sizeof(buf), "esperando sensor %lu %s (%lu s)", static_cast<unsigned long>(in->a),
                 in->b ? "on" : "off", static_cast<unsigned long>(elapsed / 1000));
      return buf;
    case WaitKind::Lock:
      return in ? "esperando bloqueo '" + in->text + "'" : "esperando bloqueo";
    case WaitKind::Ramp:
      snprintf(buf, sizeof(buf), "rampa de velocidad %ld -> %ld", static_cast<long>(rampFrom_),
               static_cast<long>(rampTo_));
      return buf;
    default:
      return "esperando";
  }
}

void Runner::touch(uint32_t loco) {
  for (uint32_t u : touched_)
    if (u == loco) return;
  touched_.push_back(loco);
}

void Runner::fail(const std::string& msg) {
  state_ = RunState::Error;
  wait_ = WaitKind::None;
  message_ = msg;
}

void Runner::jump(int target) {
  // Abandonar los bucles `repeat` de los que salimos con el salto.
  while (!repeats_.empty()) {
    const RepeatFrame& f = repeats_.back();
    const int endIdx = prog_.code[f.start].c;
    if (target > f.start && target <= endIdx) break;
    repeats_.pop_back();
  }
  pc_ = target;
}

void Runner::startWait(WaitKind kind, Host& host) {
  wait_ = kind;
  waitInstr_ = pc_;
  waitStart_ = host.nowMs();
  pc_++;
}

bool Runner::checkWait(Engine& eng, Host& host, int slot) {
  const uint32_t now = host.nowMs();
  const uint32_t elapsed = now - waitStart_;
  const Instr* in = (waitInstr_ >= 0 && waitInstr_ < static_cast<int>(prog_.code.size()))
                        ? &prog_.code[waitInstr_]
                        : nullptr;
  bool done = false;
  switch (wait_) {
    case WaitKind::None:
      done = true;
      break;
    case WaitKind::Time:
      done = elapsed >= waitMs_;
      break;
    case WaitKind::Sensor: {
      const bool want = in->b != 0;
      const int8_t st = host.sensorState(in->a);
      if (st == (want ? 1 : 0) || host.sensorEdges(in->a, want) != edges0_) {
        done = true;
      } else if (in->c > 0 && elapsed >= static_cast<uint32_t>(in->c)) {
        wait_ = WaitKind::None;
        if (in->d >= 0) {
          host.log(name_, "timeout del sensor " + toStr(static_cast<long>(in->a)) +
                              " (linea " + toStr(in->line) + "), salto a la etiqueta");
          state_ = RunState::Running;
          jump(in->d);
          return true;
        }
        fail("timeout esperando el sensor " + toStr(static_cast<long>(in->a)) + " (linea " +
             toStr(in->line) + ")");
        return false;
      }
      break;
    }
    case WaitKind::Lock:
      done = eng.tryLock(in->text, slot);
      break;
    case WaitKind::Ramp: {
      int32_t v;
      if (in->c <= 0 || elapsed >= static_cast<uint32_t>(in->c)) {
        v = rampTo_;
        done = true;
      } else {
        v = rampFrom_ + static_cast<int32_t>(static_cast<int64_t>(rampTo_ - rampFrom_) *
                                             elapsed / in->c);
      }
      if (done) {
        host.setSpeed(rampLoco_, static_cast<uint16_t>(v));
      } else if (v != rampLast_ && now - rampLastSend_ >= 100) {
        host.setSpeed(rampLoco_, static_cast<uint16_t>(v));
        rampLast_ = v;
        rampLastSend_ = now;
      }
      break;
    }
  }
  if (done) {
    wait_ = WaitKind::None;
    state_ = RunState::Running;
    return true;
  }
  state_ = RunState::Waiting;
  return false;
}

bool Runner::step(Engine& eng, Host& host, int slot, int* actions) {
  if (pc_ < 0 || pc_ >= static_cast<int>(prog_.code.size())) {
    state_ = RunState::Finished;
    message_ = "terminado";
    return false;
  }
  const Instr& in = prog_.code[pc_];
  switch (in.op) {
    case Op::Nop:
      pc_++;
      return true;
    case Op::Power:
      host.setTrackPower(in.b != 0);
      (*actions)++;
      pc_++;
      return true;
    case Op::Speed:
      touch(in.a);
      host.setSpeed(in.a, static_cast<uint16_t>(in.b));
      (*actions)++;
      pc_++;
      return true;
    case Op::Ramp:
      touch(in.a);
      rampLoco_ = in.a;
      rampFrom_ = host.getSpeed(in.a);
      rampTo_ = in.b;
      rampLast_ = rampFrom_;
      rampLastSend_ = host.nowMs() - 1000;
      (*actions)++;
      startWait(WaitKind::Ramp, host);
      return checkWait(eng, host, slot);
    case Op::Dir:
      touch(in.a);
      host.setDirection(in.a, static_cast<uint8_t>(in.b));
      (*actions)++;
      pc_++;
      return true;
    case Op::Func:
      touch(in.a);
      host.setFunction(in.a, static_cast<uint8_t>(in.b), in.c != 0);
      (*actions)++;
      pc_++;
      return true;
    case Op::EStop:
      touch(in.a);
      host.emergencyStop(in.a);
      (*actions)++;
      pc_++;
      return true;
    case Op::Halt:
      host.stopAllLocos();
      (*actions)++;
      pc_++;
      return true;
    case Op::Switch:
      host.switchAccessory(in.a, static_cast<uint8_t>(in.b));
      (*actions)++;
      pc_++;
      return true;
    case Op::Wait:
      waitMs_ = static_cast<uint32_t>(in.b);
      startWait(WaitKind::Time, host);
      return checkWait(eng, host, slot);
    case Op::WaitRandom:
      waitMs_ = host.random(static_cast<uint32_t>(in.b), static_cast<uint32_t>(in.c));
      startWait(WaitKind::Time, host);
      return checkWait(eng, host, slot);
    case Op::WaitSensor:
      edges0_ = host.sensorEdges(in.a, in.b != 0);
      startWait(WaitKind::Sensor, host);
      return checkWait(eng, host, slot);
    case Op::Goto:
      jump(in.b);
      return true;
    case Op::IfSensor:
      if (host.sensorState(in.a) == in.b)
        jump(in.c);
      else
        pc_++;
      return true;
    case Op::IfChance:
      if (static_cast<int32_t>(host.random(1, 100)) <= in.b)
        jump(in.c);
      else
        pc_++;
      return true;
    case Op::IfLocked: {
      const int owner = eng.lockOwner(in.text);
      if (owner >= 0 && owner != slot)
        jump(in.c);
      else
        pc_++;
      return true;
    }
    case Op::Repeat:
      if (in.b == 0) {
        pc_ = in.c + 1;
      } else {
        repeats_.push_back({pc_, in.b});
        pc_++;
      }
      return true;
    case Op::EndRepeat:
      if (!repeats_.empty() && repeats_.back().start == in.c) {
        RepeatFrame& f = repeats_.back();
        if (f.remaining < 0) {
          pc_ = f.start + 1;
        } else if (--f.remaining > 0) {
          pc_ = f.start + 1;
        } else {
          repeats_.pop_back();
          pc_++;
        }
      } else {
        pc_++;
      }
      return true;
    case Op::Lock:
      if (eng.tryLock(in.text, slot)) {
        pc_++;
        return true;
      }
      startWait(WaitKind::Lock, host);
      state_ = RunState::Waiting;
      return false;
    case Op::Unlock:
      eng.unlock(in.text, slot);
      pc_++;
      return true;
    case Op::Log:
      host.log(name_, in.text);
      pc_++;
      return true;
    case Op::Exit:
      state_ = RunState::Finished;
      message_ = "terminado (exit)";
      return false;
  }
  pc_++;
  return true;
}

void Runner::tick(Engine& eng, Host& host, int slot) {
  if (!active()) return;
  if (wait_ != WaitKind::None && !checkWait(eng, host, slot)) return;
  if (!active()) return;
  int actions = 0;
  for (int i = 0; i < Engine::kMaxStepsPerTick; i++) {
    if (!step(eng, host, slot, &actions)) return;
    if (actions >= 16) break;
  }
  // Bucle sin esperas: pausa implícita para no saturar el bus.
  waitMs_ = 50;
  wait_ = WaitKind::Time;
  waitInstr_ = pc_;
  waitStart_ = host.nowMs();
  state_ = RunState::Waiting;
}

// ---- Engine ------------------------------------------------------------------------

int Engine::start(const std::string& name, const std::string& source, std::string* error) {
  Program prog = compile(source, host_);
  if (!prog.ok()) {
    if (error) *error = "linea " + toStr(prog.errorLine) + ": " + prog.error;
    return -1;
  }
  if (isRunning(name)) {
    if (error) *error = "el script '" + name + "' ya esta en marcha";
    return -1;
  }
  for (int i = 0; i < kMaxRunners; i++) {
    if (!runners_[i].active()) {
      runners_[i].begin(name, std::move(prog), host_->nowMs());
      host_->log(name, "iniciado");
      return i;
    }
  }
  if (error) *error = "no hay huecos libres (maximo " + toStr(kMaxRunners) + " scripts a la vez)";
  return -1;
}

void Engine::abort(int slot, RunState finalState, const std::string& msg) {
  Runner& r = runners_[slot];
  // Seguridad: parar las locos que manejaba este script.
  for (uint32_t loco : r.touched_) host_->setSpeed(loco, 0);
  releaseLocks(slot);
  r.state_ = finalState;
  r.wait_ = Runner::WaitKind::None;
  r.message_ = msg;
  host_->log(r.name_, msg);
}

bool Engine::stop(int slot) {
  if (slot < 0 || slot >= kMaxRunners || !runners_[slot].active()) return false;
  abort(slot, RunState::Stopped, "detenido");
  return true;
}

bool Engine::stopByName(const std::string& name) {
  for (int i = 0; i < kMaxRunners; i++)
    if (runners_[i].active() && runners_[i].name_ == name) return stop(i);
  return false;
}

void Engine::stopAll() {
  for (int i = 0; i < kMaxRunners; i++) stop(i);
}

void Engine::tick() {
  for (int i = 0; i < kMaxRunners; i++) {
    Runner& r = runners_[i];
    if (!r.active()) continue;
    r.tick(*this, *host_, i);
    if (r.state_ == RunState::Error) {
      abort(i, RunState::Error, "ERROR: " + r.message_);
    } else if (r.state_ == RunState::Finished) {
      releaseLocks(i);
      host_->log(r.name_, r.message_);
    }
  }
}

bool Engine::isRunning(const std::string& name) const {
  for (const auto& r : runners_)
    if (r.active() && r.name_ == name) return true;
  return false;
}

int Engine::activeCount() const {
  int n = 0;
  for (const auto& r : runners_)
    if (r.active()) n++;
  return n;
}

std::string Engine::exec(const std::string& line) {
  Program prog = compile(line, host_);
  if (!prog.ok()) return prog.error;
  for (const Instr& in : prog.code) {
    switch (in.op) {
      case Op::Nop: break;
      case Op::Power: host_->setTrackPower(in.b != 0); break;
      case Op::Speed: host_->setSpeed(in.a, static_cast<uint16_t>(in.b)); break;
      case Op::Dir: host_->setDirection(in.a, static_cast<uint8_t>(in.b)); break;
      case Op::Func: host_->setFunction(in.a, static_cast<uint8_t>(in.b), in.c != 0); break;
      case Op::EStop: host_->emergencyStop(in.a); break;
      case Op::Halt: host_->stopAllLocos(); break;
      case Op::Switch: host_->switchAccessory(in.a, static_cast<uint8_t>(in.b)); break;
      case Op::Log: host_->log("cmd", in.text); break;
      default:
        return "orden no permitida en modo inmediato (usa un script para esperas, saltos, "
               "rampas o bloqueos)";
    }
  }
  return "";
}

bool Engine::tryLock(const std::string& name, int slot) {
  for (const auto& l : locks_)
    if (l.name == name) return l.owner == slot;
  locks_.push_back({name, slot});
  return true;
}

void Engine::unlock(const std::string& name, int slot) {
  for (size_t i = 0; i < locks_.size(); i++) {
    if (locks_[i].name == name && locks_[i].owner == slot) {
      locks_.erase(locks_.begin() + static_cast<long>(i));
      return;
    }
  }
}

void Engine::releaseLocks(int slot) {
  for (size_t i = 0; i < locks_.size();) {
    if (locks_[i].owner == slot)
      locks_.erase(locks_.begin() + static_cast<long>(i));
    else
      i++;
  }
}

int Engine::lockOwner(const std::string& name) const {
  for (const auto& l : locks_)
    if (l.name == name) return l.owner;
  return -1;
}

}  // namespace automation
