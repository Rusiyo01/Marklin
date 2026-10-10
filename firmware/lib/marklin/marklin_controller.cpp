#include "marklin_controller.h"

#include <string.h>

namespace marklin {

void Controller::setUid(uint32_t uid) {
  uid_ = uid;
  hash_ = hashFromUid(uid);
}

bool Controller::send(const CanFrame& f) {
  bool ok = sink_ && sink_->send(f);
  if (ok) {
    framesTx_++;
  } else {
    txErrors_++;
  }
  if (onFrame) onFrame(f, true);
  return ok;
}

void Controller::tick(uint32_t nowMs) {
  now_ = nowMs;
  for (auto& p : pending_) {
    // Comparación con resta para soportar el desbordamiento de millis().
    if (p.active && static_cast<int32_t>(nowMs - p.offAtMs) >= 0) {
      p.active = false;
      send(buildAccessory(hash_, p.uid, p.position, 0));
    }
  }
}

// ---- Órdenes -----------------------------------------------------------------

bool Controller::setTrackPower(bool on) {
  bool ok = send(on ? buildGo(hash_) : buildStop(hash_));
  if (ok) {
    power_ = on ? PowerState::On : PowerState::Off;
    if (on) overload_ = false;
  }
  return ok;
}

bool Controller::queryTrackPower() { return send(buildPowerQuery(hash_)); }

bool Controller::setSpeed(uint32_t locoUid, uint16_t speed) {
  if (speed > kMaxSpeed) speed = kMaxSpeed;
  bool ok = send(buildSpeed(hash_, locoUid, speed));
  if (ok) applyLocoSpeed(locoUid, speed);
  return ok;
}

bool Controller::setDirection(uint32_t locoUid, Direction dir) {
  LocoState* l = touchLoco(locoUid);
  if (dir == DIR_TOGGLE) {
    dir = (l && l->direction == DIR_REVERSE) ? DIR_FORWARD
          : (l && l->direction == DIR_FORWARD) ? DIR_REVERSE
                                               : DIR_TOGGLE;
  }
  if (dir == DIR_KEEP) return true;
  if (l && l->speed != 0) setSpeed(locoUid, 0);
  bool ok = send(buildDirection(hash_, locoUid, dir));
  if (ok && l) {
    if (dir == DIR_FORWARD || dir == DIR_REVERSE) l->direction = dir;
    l->speed = 0;
    l->lastChangeMs = now_;
  }
  return ok;
}

bool Controller::setFunction(uint32_t locoUid, uint8_t function, bool on) {
  bool ok = send(buildFunction(hash_, locoUid, function, on ? 1 : 0));
  if (ok) applyLocoFunction(locoUid, function, on ? 1 : 0);
  return ok;
}

bool Controller::emergencyStop(uint32_t locoUid) {
  bool ok = send(buildLocoEmergencyStop(hash_, locoUid));
  if (ok) applyLocoSpeed(locoUid, 0);
  return ok;
}

int Controller::stopAllLocos() {
  int n = 0;
  for (int i = 0; i < locoCount_; i++) {
    if (locos_[i].speed != 0) {
      setSpeed(locos_[i].uid, 0);
      n++;
    }
  }
  return n;
}

bool Controller::switchAccessory(uint32_t accUid, uint8_t position,
                                 uint16_t pulseMs) {
  bool ok = send(buildAccessory(hash_, accUid, position, 1));
  if (!ok) return false;
  updateAccessory(accUid, position);
  if (pulseMs == 0) pulseMs = 1;
  // Si ya había un apagado pendiente para este accesorio, se reutiliza.
  PendingAccessory* slot = nullptr;
  for (auto& p : pending_) {
    if (p.active && p.uid == accUid) {
      // Cambio de posición antes de apagar la anterior: apagarla ya.
      if (p.position != position) send(buildAccessory(hash_, p.uid, p.position, 0));
      slot = &p;
      break;
    }
  }
  if (!slot) {
    for (auto& p : pending_) {
      if (!p.active) {
        slot = &p;
        break;
      }
    }
  }
  if (!slot) {
    // Cola llena: apagado inmediato para no dejar bobinas activadas.
    send(buildAccessory(hash_, accUid, position, 0));
    return true;
  }
  slot->active = true;
  slot->uid = accUid;
  slot->position = position;
  slot->offAtMs = now_ + pulseMs;
  return true;
}

bool Controller::ping() { return send(buildPing(hash_)); }

bool Controller::queryLoco(uint32_t locoUid) {
  bool a = send(buildSpeedQuery(hash_, locoUid));
  bool b = send(buildDirectionQuery(hash_, locoUid));
  return a && b;
}

bool Controller::startGleisbox(uint8_t protocolMask) {
  bool a = send(buildGleisboxStart(hash_));
  bool b = send(buildEnableProtocols(hash_, protocolMask));
  return a && b;
}

void Controller::setSensor(uint32_t id, bool active) { updateSensor(id, active); }

// ---- Recepción -----------------------------------------------------------------

void Controller::handleFrame(const CanFrame& f) {
  framesRx_++;
  if (onFrame) onFrame(f, false);

  if (!isResponse(f)) {
    foreignRequests_++;
    // Otro equipo usa nuestro hash en una petición: elegimos otro.
    if (hashOf(f) == hash_) hash_ = sanitizeHash(static_cast<uint16_t>(hash_ + 0x0401));
  }

  const uint8_t cmd = commandOf(f);
  const uint32_t uid = f.dlc >= 4 ? readBe32(f.data) : 0;

  switch (cmd) {
    case CMD_SYSTEM:
      if (f.dlc < 5) break;
      switch (f.data[4]) {
        case SYS_STOP:
          if (uid == 0) power_ = PowerState::Off;
          break;
        case SYS_GO:
          if (uid == 0) {
            power_ = PowerState::On;
            overload_ = false;
          }
          break;
        case SYS_HALT:
          for (int i = 0; i < locoCount_; i++) locos_[i].speed = 0;
          break;
        case SYS_LOCO_EMERGENCY:
          if (uid == 0) {
            for (int i = 0; i < locoCount_; i++) locos_[i].speed = 0;
          } else {
            applyLocoSpeed(uid, 0);
          }
          break;
        case SYS_OVERLOAD:
          overload_ = true;
          power_ = PowerState::Off;
          break;
        default:
          break;
      }
      break;
    case CMD_LOCO_SPEED:
      if (f.dlc >= 6) applyLocoSpeed(uid, readBe16(f.data + 4));
      break;
    case CMD_LOCO_DIRECTION:
      if (f.dlc >= 5) applyLocoDirection(uid, f.data[4]);
      break;
    case CMD_LOCO_FUNCTION:
      if (f.dlc >= 6) applyLocoFunction(uid, f.data[4], f.data[5]);
      break;
    case CMD_ACCESSORY:
      if (f.dlc >= 6 && f.data[5]) updateAccessory(uid, f.data[4]);
      break;
    case CMD_S88_EVENT:
      if (f.dlc >= 6) {
        const uint16_t module = readBe16(f.data);
        const uint16_t contact = readBe16(f.data + 2);
        const uint32_t id = module ? module * 1000u + contact : contact;
        updateSensor(id, f.data[5] != 0);
      }
      break;
    case CMD_PING:
      if (isResponse(f) && f.dlc >= 8) updateDevice(f);
      break;
    default:
      break;
  }
}

// ---- Estado ----------------------------------------------------------------------

const LocoState* Controller::findLoco(uint32_t uid) const {
  for (int i = 0; i < locoCount_; i++)
    if (locos_[i].uid == uid) return &locos_[i];
  return nullptr;
}

LocoState* Controller::touchLoco(uint32_t uid) {
  if (!isLocoUid(uid)) return nullptr;
  for (int i = 0; i < locoCount_; i++)
    if (locos_[i].uid == uid) return &locos_[i];
  if (locoCount_ < kMaxLocos) {
    LocoState& l = locos_[locoCount_++];
    l = LocoState();
    l.uid = uid;
    l.lastChangeMs = now_;
    return &l;
  }
  // Tabla llena: reciclar la loco parada, no fijada, más antigua.
  int victim = -1;
  for (int i = 0; i < locoCount_; i++) {
    const LocoState& l = locos_[i];
    if (l.pinned || l.speed != 0) continue;
    if (victim < 0 ||
        static_cast<int32_t>(l.lastChangeMs - locos_[victim].lastChangeMs) < 0)
      victim = i;
  }
  if (victim < 0) return nullptr;
  locos_[victim] = LocoState();
  locos_[victim].uid = uid;
  locos_[victim].lastChangeMs = now_;
  return &locos_[victim];
}

void Controller::setLocoName(uint32_t uid, const char* name) {
  LocoState* l = touchLoco(uid);
  if (!l) return;
  strncpy(l->name, name ? name : "", sizeof(l->name) - 1);
  l->name[sizeof(l->name) - 1] = 0;
  l->pinned = true;
}

void Controller::applyLocoSpeed(uint32_t uid, uint16_t speed) {
  LocoState* l = touchLoco(uid);
  if (!l) return;
  l->speed = speed > kMaxSpeed ? kMaxSpeed : speed;
  l->lastChangeMs = now_;
}

void Controller::applyLocoDirection(uint32_t uid, uint8_t dir) {
  if (dir != DIR_FORWARD && dir != DIR_REVERSE) return;
  LocoState* l = touchLoco(uid);
  if (!l) return;
  if (l->direction != dir) {
    // Un cambio de sentido detiene la loco (comportamiento Märklin).
    l->direction = dir;
    l->speed = 0;
  }
  l->lastChangeMs = now_;
}

void Controller::applyLocoFunction(uint32_t uid, uint8_t fn, uint8_t value) {
  if (fn >= 32) return;
  LocoState* l = touchLoco(uid);
  if (!l) return;
  if (value)
    l->functions |= (1u << fn);
  else
    l->functions &= ~(1u << fn);
  l->lastChangeMs = now_;
}

const SensorState* Controller::findSensor(uint32_t id) const {
  for (int i = 0; i < sensorCount_; i++)
    if (sensors_[i].id == id) return &sensors_[i];
  return nullptr;
}

int8_t Controller::sensorState(uint32_t id) const {
  const SensorState* s = findSensor(id);
  return s ? s->state : -1;
}

SensorState* Controller::touchSensor(uint32_t id) {
  for (int i = 0; i < sensorCount_; i++)
    if (sensors_[i].id == id) return &sensors_[i];
  if (sensorCount_ >= kMaxSensors) return nullptr;
  SensorState& s = sensors_[sensorCount_++];
  s = SensorState();
  s.id = id;
  return &s;
}

void Controller::declareSensor(uint32_t id) { touchSensor(id); }

void Controller::updateSensor(uint32_t id, bool active) {
  SensorState* s = touchSensor(id);
  if (!s) return;
  const int8_t v = active ? 1 : 0;
  if (s->state == v) return;
  if (active)
    s->onCount++;
  else if (s->state == 1)
    s->offCount++;
  s->state = v;
  s->changedMs = now_;
}

void Controller::updateAccessory(uint32_t uid, uint8_t position) {
  for (int i = 0; i < accessoryCount_; i++) {
    if (accessories_[i].uid == uid) {
      accessories_[i].position = position;
      accessories_[i].changedMs = now_;
      return;
    }
  }
  if (accessoryCount_ >= kMaxAccessories) return;
  AccessoryState& a = accessories_[accessoryCount_++];
  a.uid = uid;
  a.position = position;
  a.changedMs = now_;
}

void Controller::updateDevice(const CanFrame& f) {
  const uint32_t uid = readBe32(f.data);
  DeviceInfo* d = nullptr;
  for (int i = 0; i < deviceCount_; i++)
    if (devices_[i].uid == uid) d = &devices_[i];
  if (!d) {
    if (deviceCount_ >= kMaxDevices) return;
    d = &devices_[deviceCount_++];
    d->uid = uid;
  }
  d->versionHigh = f.data[4];
  d->versionLow = f.data[5];
  d->type = readBe16(f.data + 6);
  d->lastSeenMs = now_;
}

bool Controller::ms2Present() const {
  for (int i = 0; i < deviceCount_; i++)
    if (devices_[i].type >= 0x0030 && devices_[i].type <= 0x0034) return true;
  return false;
}

}  // namespace marklin
