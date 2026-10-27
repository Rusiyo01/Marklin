#include "marklin_can.h"

#include <stdio.h>
#include <string.h>

namespace marklin {

uint16_t sanitizeHash(uint16_t hash) {
  return static_cast<uint16_t>((hash & 0xFF7F) | 0x0300);
}

uint16_t hashFromUid(uint32_t uid) {
  uint16_t hash = static_cast<uint16_t>((uid >> 16) ^ (uid & 0xFFFF));
  return static_cast<uint16_t>((((hash << 3) & 0xFF00) | 0x0300) |
                               (hash & 0x7F));
}

uint32_t locoUid(Protocol proto, uint32_t address) {
  switch (proto) {
    case Protocol::MM2: return UID_MM2_BASE + address;
    case Protocol::SX1: return UID_SX1_BASE + address;
    case Protocol::MFX: return UID_MFX_BASE + address;
    case Protocol::DCC: return UID_DCC_BASE + address;
    case Protocol::RAW: return address;
  }
  return address;
}

uint32_t accessoryUid(Protocol proto, uint32_t address) {
  if (address == 0) address = 1;
  switch (proto) {
    case Protocol::DCC: return UID_ACC_DCC_BASE + address - 1;
    case Protocol::RAW: return address;
    default: return UID_ACC_MM_BASE + address - 1;
  }
}

bool isLocoUid(uint32_t uid) {
  Protocol p;
  uint32_t a;
  return decodeLocoUid(uid, &p, &a);
}

bool isAccessoryUid(uint32_t uid) { return uid >= 0x3000 && uid < 0x4000; }

bool decodeLocoUid(uint32_t uid, Protocol* proto, uint32_t* address) {
  if (uid >= 1 && uid <= 0x03FF) {
    *proto = Protocol::MM2;
    *address = uid;
  } else if (uid >= UID_SX1_BASE && uid < 0x0C00) {
    *proto = Protocol::SX1;
    *address = uid - UID_SX1_BASE;
  } else if (uid >= UID_MFX_BASE && uid < 0x8000) {
    *proto = Protocol::MFX;
    *address = uid - UID_MFX_BASE;
  } else if (uid >= UID_DCC_BASE && uid <= 0xFFFF) {
    *proto = Protocol::DCC;
    *address = uid - UID_DCC_BASE;
  } else {
    return false;
  }
  return true;
}

bool decodeAccessoryUid(uint32_t uid, Protocol* proto, uint32_t* address) {
  if (uid >= UID_ACC_MM_BASE && uid < 0x3400) {
    *proto = Protocol::MM2;
    *address = uid - UID_ACC_MM_BASE + 1;
    return true;
  }
  if (uid >= UID_ACC_DCC_BASE && uid < 0x4000) {
    *proto = Protocol::DCC;
    *address = uid - UID_ACC_DCC_BASE + 1;
    return true;
  }
  return false;
}

const char* protocolName(Protocol proto) {
  switch (proto) {
    case Protocol::MM2: return "mm2";
    case Protocol::MFX: return "mfx";
    case Protocol::DCC: return "dcc";
    case Protocol::SX1: return "sx1";
    case Protocol::RAW: return "uid";
  }
  return "?";
}

void formatLocoUid(uint32_t uid, char* out, size_t len) {
  Protocol p;
  uint32_t a;
  if (decodeLocoUid(uid, &p, &a)) {
    snprintf(out, len, "%s:%lu", protocolName(p), static_cast<unsigned long>(a));
  } else {
    snprintf(out, len, "0x%08lX", static_cast<unsigned long>(uid));
  }
}

// ---- Constructores ---------------------------------------------------------

static CanFrame makeFrame(uint8_t cmd, uint16_t hash, uint8_t dlc) {
  CanFrame f;
  f.id = makeId(cmd, false, hash);
  f.dlc = dlc;
  return f;
}

CanFrame buildSystem(uint16_t hash, uint32_t uid, uint8_t sub) {
  CanFrame f = makeFrame(CMD_SYSTEM, hash, 5);
  writeBe32(f.data, uid);
  f.data[4] = sub;
  return f;
}

CanFrame buildLocoEmergencyStop(uint16_t hash, uint32_t uid) {
  return buildSystem(hash, uid, SYS_LOCO_EMERGENCY);
}

CanFrame buildPowerQuery(uint16_t hash) { return makeFrame(CMD_SYSTEM, hash, 4); }

CanFrame buildEnableProtocols(uint16_t hash, uint8_t mask) {
  CanFrame f = makeFrame(CMD_SYSTEM, hash, 6);
  f.data[4] = SYS_ENABLE_PROTOCOLS;
  f.data[5] = mask;
  return f;
}

CanFrame buildSpeed(uint16_t hash, uint32_t uid, uint16_t speed) {
  if (speed > kMaxSpeed) speed = kMaxSpeed;
  CanFrame f = makeFrame(CMD_LOCO_SPEED, hash, 6);
  writeBe32(f.data, uid);
  writeBe16(f.data + 4, speed);
  return f;
}

CanFrame buildSpeedQuery(uint16_t hash, uint32_t uid) {
  CanFrame f = makeFrame(CMD_LOCO_SPEED, hash, 4);
  writeBe32(f.data, uid);
  return f;
}

CanFrame buildDirection(uint16_t hash, uint32_t uid, Direction dir) {
  CanFrame f = makeFrame(CMD_LOCO_DIRECTION, hash, 5);
  writeBe32(f.data, uid);
  f.data[4] = dir;
  return f;
}

CanFrame buildDirectionQuery(uint16_t hash, uint32_t uid) {
  CanFrame f = makeFrame(CMD_LOCO_DIRECTION, hash, 4);
  writeBe32(f.data, uid);
  return f;
}

CanFrame buildFunction(uint16_t hash, uint32_t uid, uint8_t function,
                       uint8_t value) {
  CanFrame f = makeFrame(CMD_LOCO_FUNCTION, hash, 6);
  writeBe32(f.data, uid);
  f.data[4] = function;
  f.data[5] = value;
  return f;
}

CanFrame buildAccessory(uint16_t hash, uint32_t accUid, uint8_t position,
                        uint8_t power) {
  CanFrame f = makeFrame(CMD_ACCESSORY, hash, 6);
  writeBe32(f.data, accUid);
  f.data[4] = position;
  f.data[5] = power;
  return f;
}

CanFrame buildPing(uint16_t hash) { return makeFrame(CMD_PING, hash, 0); }

CanFrame buildReadConfig(uint16_t hash, uint32_t locoUid, uint16_t cv, uint8_t index) {
  CanFrame f = makeFrame(CMD_READ_CONFIG, hash, 7);
  writeBe32(f.data, locoUid);
  writeBe16(f.data + 4, static_cast<uint16_t>((index & 0x3F) << 10 | (cv & 0x3FF)));
  f.data[6] = 1;  // número de bytes
  return f;
}

CanFrame buildWriteConfig(uint16_t hash, uint32_t locoUid, uint16_t cv, uint8_t index,
                          uint8_t value, bool mainTrack) {
  CanFrame f = makeFrame(CMD_WRITE_CONFIG, hash, 8);
  writeBe32(f.data, locoUid);
  writeBe16(f.data + 4, static_cast<uint16_t>((index & 0x3F) << 10 | (cv & 0x3FF)));
  f.data[6] = value;
  f.data[7] = mainTrack ? 0x80 : 0x00;  // modo byte directo
  return f;
}

CanFrame buildGleisboxStart(uint16_t hash) {
  CanFrame f = makeFrame(CMD_BOOTLOADER, hash, 5);
  f.data[4] = 0x11;
  return f;
}

// ---- Descripción -------------------------------------------------------------

const char* deviceTypeName(uint16_t type, uint32_t uid) {
  switch (type) {
    case 0x0000:
      return ((uid & 0xFF000000u) == 0x42000000u) ? "Booster" : "GFP";
    case 0x0010:
    case 0x0011: return "Gleisbox";
    case 0x0020: return "Connect 6021";
    case 0x0030:
    case 0x0031:
    case 0x0032:
    case 0x0033:
    case 0x0034: return "MS2";
    case 0x0040: return "Link S88";
    case 0xEEEE: return "CS2 software";
    case 0xFFF0:
    case 0xFFFF: return "CS2/CS3 (master)";
    default: return "unknown";
  }
}

static const char* dirName(uint8_t d) {
  switch (d) {
    case DIR_KEEP: return "keep";
    case DIR_FORWARD: return "forward";
    case DIR_REVERSE: return "reverse";
    case DIR_TOGGLE: return "toggle";
    default: return "?";
  }
}

void describeFrame(const CanFrame& f, char* out, size_t len) {
  const uint8_t cmd = commandOf(f);
  const char* r = isResponse(f) ? "R " : "";
  const uint32_t uid = f.dlc >= 4 ? readBe32(f.data) : 0;
  char loco[24];
  formatLocoUid(uid, loco, sizeof(loco));

  switch (cmd) {
    case CMD_SYSTEM: {
      if (f.dlc < 5) {
        snprintf(out, len, "%sSystem: status query", r);
        return;
      }
      const uint8_t sub = f.data[4];
      switch (sub) {
        case SYS_STOP: snprintf(out, len, "%sSystem: STOP (no track power)", r); return;
        case SYS_GO: snprintf(out, len, "%sSystem: GO (track power on)", r); return;
        case SYS_HALT: snprintf(out, len, "%sSystem: HALT (locos stopped)", r); return;
        case SYS_LOCO_EMERGENCY:
          snprintf(out, len, "%sSystem: emergency stop loco %s", r, uid ? loco : "all");
          return;
        case SYS_ENABLE_PROTOCOLS:
          snprintf(out, len, "%sSystem: enable protocols 0x%02X", r,
                   f.dlc > 5 ? f.data[5] : 0);
          return;
        case SYS_OVERLOAD:
          snprintf(out, len, "%sSystem: OVERLOAD channel %u", r, f.dlc > 5 ? f.data[5] : 0);
          return;
        default:
          snprintf(out, len, "%sSystem: subcommand 0x%02X uid 0x%08lX", r, sub,
                   static_cast<unsigned long>(uid));
          return;
      }
    }
    case CMD_LOCO_SPEED:
      if (f.dlc >= 6)
        snprintf(out, len, "%sLoco %s speed %u", r, loco, readBe16(f.data + 4));
      else
        snprintf(out, len, "%sLoco %s speed query", r, loco);
      return;
    case CMD_LOCO_DIRECTION:
      if (f.dlc >= 5)
        snprintf(out, len, "%sLoco %s direction %s", r, loco, dirName(f.data[4]));
      else
        snprintf(out, len, "%sLoco %s direction query", r, loco);
      return;
    case CMD_LOCO_FUNCTION:
      if (f.dlc >= 6)
        snprintf(out, len, "%sLoco %s F%u = %u", r, loco, f.data[4], f.data[5]);
      else if (f.dlc == 5)
        snprintf(out, len, "%sLoco %s query F%u", r, loco, f.data[4]);
      else
        snprintf(out, len, "%sLoco function (dlc %u)", r, f.dlc);
      return;
    case CMD_ACCESSORY: {
      Protocol p;
      uint32_t a;
      if (f.dlc >= 6 && decodeAccessoryUid(uid, &p, &a))
        snprintf(out, len, "%sAccessory %s:%lu position %u current %u", r,
                 p == Protocol::DCC ? "dcc" : "mm", static_cast<unsigned long>(a),
                 f.data[4], f.data[5]);
      else
        snprintf(out, len, "%sAccessory uid 0x%08lX", r, static_cast<unsigned long>(uid));
      return;
    }
    case CMD_S88_EVENT:
      if (f.dlc >= 6)
        snprintf(out, len, "%sS88 module %u contact %u: %u -> %u", r, readBe16(f.data),
                 readBe16(f.data + 2), f.data[4], f.data[5]);
      else
        snprintf(out, len, "%sS88 query module %u contact %u", r,
                 f.dlc >= 2 ? readBe16(f.data) : 0, f.dlc >= 4 ? readBe16(f.data + 2) : 0);
      return;
    case CMD_PING:
      if (isResponse(f) && f.dlc >= 8)
        snprintf(out, len, "R Ping: %s uid 0x%08lX version %u.%u",
                 deviceTypeName(readBe16(f.data + 6), uid), static_cast<unsigned long>(uid),
                 f.data[4], f.data[5]);
      else
        snprintf(out, len, "%sPing", r);
      return;
    case CMD_BOOTLOADER:
      if (f.dlc == 5 && f.data[4] == 0x11)
        snprintf(out, len, "%sGleisbox start (bootloader GO)", r);
      else
        snprintf(out, len, "%sBootloader (dlc %u)", r, f.dlc);
      return;
    case CMD_LOCO_DISCOVERY: snprintf(out, len, "%sLoco discovery", r); return;
    case CMD_MFX_BIND: snprintf(out, len, "%sMFX bind", r); return;
    case CMD_MFX_VERIFY: snprintf(out, len, "%sMFX verify", r); return;
    case CMD_READ_CONFIG: snprintf(out, len, "%sRead CV", r); return;
    case CMD_WRITE_CONFIG: snprintf(out, len, "%sWrite CV", r); return;
    case CMD_CONFIG_DATA_REQUEST: snprintf(out, len, "%sConfig data request", r); return;
    case CMD_CONFIG_DATA_STREAM: snprintf(out, len, "%sConfig data stream", r); return;
    default: {
      int n = snprintf(out, len, "%sCommand 0x%02X dlc %u:", r, cmd, f.dlc);
      for (uint8_t i = 0; i < f.dlc && i < 8 && n > 0 && static_cast<size_t>(n) < len; i++)
        n += snprintf(out + n, len - n, " %02X", f.data[i]);
      return;
    }
  }
}

}  // namespace marklin
