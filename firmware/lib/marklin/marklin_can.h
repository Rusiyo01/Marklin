// Protocolo CAN de Märklin (MS2 / Gleisbox 60113-60116 / CS2 / CS3).
//
// Biblioteca portable (sin dependencias de Arduino) para construir e
// interpretar tramas del bus CAN de Märklin. Se compila tanto en el ESP32
// como en el PC (tests).
//
// Resumen del formato (ver docs/investigacion.md):
//   - CAN 2.0B, identificador extendido de 29 bits, 250 kbit/s.
//   - ID = prioridad(4) | comando(8) | bit de respuesta(1) | hash(16)
//   - Datos: 0..8 bytes, enteros en big-endian. Casi todos los comandos
//     empiezan con un UID de 32 bits (dirección de loco, accesorio o equipo).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace marklin {

constexpr uint32_t kBitrate = 250000;

struct CanFrame {
  uint32_t id = 0;     // 29 bits
  uint8_t dlc = 0;     // 0..8
  uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

// Comandos (byte "comando" del identificador).
enum Command : uint8_t {
  CMD_SYSTEM = 0x00,
  CMD_LOCO_DISCOVERY = 0x01,
  CMD_MFX_BIND = 0x02,
  CMD_MFX_VERIFY = 0x03,
  CMD_LOCO_SPEED = 0x04,
  CMD_LOCO_DIRECTION = 0x05,
  CMD_LOCO_FUNCTION = 0x06,
  CMD_READ_CONFIG = 0x07,
  CMD_WRITE_CONFIG = 0x08,
  CMD_ACCESSORY = 0x0B,
  CMD_ACCESSORY_CONFIG = 0x0C,
  CMD_S88_POLL = 0x10,
  CMD_S88_EVENT = 0x11,
  CMD_SX1_EVENT = 0x12,
  CMD_PING = 0x18,
  CMD_UPDATE_OFFER = 0x19,
  CMD_READ_CONFIG_DATA = 0x1A,
  CMD_BOOTLOADER = 0x1B,
  CMD_BOOTLOADER_TRACK = 0x1C,
  CMD_STATUS_CONFIG = 0x1D,
  CMD_CONFIG_DATA_REQUEST = 0x20,
  CMD_CONFIG_DATA_STREAM = 0x21,
};

// Subcomandos de CMD_SYSTEM (byte de datos 4).
enum SystemSub : uint8_t {
  SYS_STOP = 0x00,             // corta la corriente de vía
  SYS_GO = 0x01,               // da corriente a la vía
  SYS_HALT = 0x02,             // todas las locos paran (vía con corriente)
  SYS_LOCO_EMERGENCY = 0x03,   // parada de emergencia de una loco
  SYS_LOCO_CYCLE_END = 0x04,
  SYS_LOCO_PROTOCOL = 0x05,
  SYS_ACCESSORY_TIME = 0x06,
  SYS_MFX_FAST_READ = 0x07,
  SYS_ENABLE_PROTOCOLS = 0x08,  // habilita protocolos de vía (MM2/mfx/DCC)
  SYS_MFX_REREG_COUNTER = 0x09,
  SYS_OVERLOAD = 0x0A,          // sobrecarga (cortocircuito)
  SYS_STATUS = 0x0B,
  SYS_DEVICE_ID = 0x0C,
  SYS_MODEL_TIME = 0x20,
  SYS_RESET = 0x80,
};

// Bits para SYS_ENABLE_PROTOCOLS.
constexpr uint8_t PROTO_MASK_MM2 = 0x01;
constexpr uint8_t PROTO_MASK_MFX = 0x02;
constexpr uint8_t PROTO_MASK_DCC = 0x04;

// Sentido de marcha (CMD_LOCO_DIRECTION).
enum Direction : uint8_t {
  DIR_KEEP = 0,
  DIR_FORWARD = 1,
  DIR_REVERSE = 2,
  DIR_TOGGLE = 3,
};

// Posiciones de accesorio (CMD_ACCESSORY, byte 4).
enum AccessoryPosition : uint8_t {
  ACC_ROUND = 0,     // desviado / rojo / Hp0
  ACC_STRAIGHT = 1,  // recto / verde / Hp1
  ACC_YELLOW = 2,    // 3ª salida (desvío triple, señal Hp2)
  ACC_WHITE = 3,     // 4ª salida (Sh0)
};

// Protocolo de una loco o accesorio, para calcular su UID.
enum class Protocol : uint8_t { MM2, MFX, DCC, SX1, RAW };

// Rangos de UID (ver tabla en docs/investigacion.md).
constexpr uint32_t UID_MM2_BASE = 0x0000;
constexpr uint32_t UID_SX1_BASE = 0x0800;
constexpr uint32_t UID_ACC_MM_BASE = 0x3000;   // dirección 1 = 0x3000
constexpr uint32_t UID_ACC_DCC_BASE = 0x3800;  // dirección 1 = 0x3800
constexpr uint32_t UID_MFX_BASE = 0x4000;
constexpr uint32_t UID_DCC_BASE = 0xC000;

constexpr uint16_t kMaxSpeed = 1000;  // escala de velocidad del protocolo

// ---- Identificador -------------------------------------------------------

// Hash de 16 bits a partir del UID del equipo. Los bits 7..9 quedan en
// 0b110 para distinguir las tramas del protocolo nuevo (MS2/CS2) de las
// antiguas de la CS1.
uint16_t hashFromUid(uint32_t uid);

// Fuerza el patrón 0b110 en los bits 7..9 de un hash cualquiera.
uint16_t sanitizeHash(uint16_t hash);

inline uint32_t makeId(uint8_t command, bool response, uint16_t hash,
                       uint8_t prio = 0) {
  return (static_cast<uint32_t>(prio & 0x0F) << 25) |
         (static_cast<uint32_t>(command) << 17) |
         (response ? 0x10000u : 0u) | hash;
}
inline uint8_t commandOf(const CanFrame& f) { return (f.id >> 17) & 0xFF; }
inline bool isResponse(const CanFrame& f) { return (f.id >> 16) & 0x01; }
inline uint16_t hashOf(const CanFrame& f) { return f.id & 0xFFFF; }

inline uint32_t readBe32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
inline uint16_t readBe16(const uint8_t* p) {
  return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
inline void writeBe32(uint8_t* p, uint32_t v) {
  p[0] = v >> 24;
  p[1] = v >> 16;
  p[2] = v >> 8;
  p[3] = v;
}
inline void writeBe16(uint8_t* p, uint16_t v) {
  p[0] = v >> 8;
  p[1] = v;
}

// ---- Direcciones -----------------------------------------------------------

// UID de una locomotora. `address` es la dirección del decodificador
// (MM2: 1..255, DCC: 1..10239, mfx: SID asignado por la MS2).
uint32_t locoUid(Protocol proto, uint32_t address);

// UID de un accesorio (desvío/señal). `address` empieza en 1, como en la MS2.
// Solo MM2 (por defecto) y DCC son válidos.
uint32_t accessoryUid(Protocol proto, uint32_t address);

bool isLocoUid(uint32_t uid);
bool isAccessoryUid(uint32_t uid);

// Inversa de locoUid/accessoryUid. Devuelve false si el UID no es de loco
// o accesorio conocido.
bool decodeLocoUid(uint32_t uid, Protocol* proto, uint32_t* address);
bool decodeAccessoryUid(uint32_t uid, Protocol* proto, uint32_t* address);

const char* protocolName(Protocol proto);

// Texto del tipo "mm2:24", "dcc:3", "mfx:5" o "0x00004005".
void formatLocoUid(uint32_t uid, char* out, size_t len);

// ---- Constructores de tramas ---------------------------------------------

CanFrame buildSystem(uint16_t hash, uint32_t uid, uint8_t sub);
inline CanFrame buildStop(uint16_t hash) { return buildSystem(hash, 0, SYS_STOP); }
inline CanFrame buildGo(uint16_t hash) { return buildSystem(hash, 0, SYS_GO); }
inline CanFrame buildHalt(uint16_t hash) { return buildSystem(hash, 0, SYS_HALT); }
CanFrame buildLocoEmergencyStop(uint16_t hash, uint32_t locoUid);
// Consulta del estado STOP/GO (la Gleisbox responde con el subcomando actual).
CanFrame buildPowerQuery(uint16_t hash);
CanFrame buildEnableProtocols(uint16_t hash, uint8_t mask);

CanFrame buildSpeed(uint16_t hash, uint32_t locoUid, uint16_t speed);
CanFrame buildSpeedQuery(uint16_t hash, uint32_t locoUid);
CanFrame buildDirection(uint16_t hash, uint32_t locoUid, Direction dir);
CanFrame buildDirectionQuery(uint16_t hash, uint32_t locoUid);
CanFrame buildFunction(uint16_t hash, uint32_t locoUid, uint8_t function,
                       uint8_t value);

// `power`: 1 = activar bobina, 0 = desactivar.
CanFrame buildAccessory(uint16_t hash, uint32_t accUid, uint8_t position,
                        uint8_t power);

CanFrame buildPing(uint16_t hash);

// Trama "mágica" que la MS2 envía al arrancar para que la Gleisbox salga
// del bootloader e inicie su aplicación (comando 0x1B, dato 0x11).
CanFrame buildGleisboxStart(uint16_t hash);

// ---- Interpretación ---------------------------------------------------------

// Escribe una descripción legible (en castellano) de la trama.
void describeFrame(const CanFrame& f, char* out, size_t len);

// Nombre del tipo de equipo a partir del identificador de 16 bits que
// envían en la respuesta al ping (bytes 6..7).
const char* deviceTypeName(uint16_t deviceType, uint32_t uid);

}  // namespace marklin
