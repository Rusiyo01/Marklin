// Controlador de alto nivel del bus Märklin.
//
// - Envía órdenes (corriente de vía, velocidad, sentido, funciones,
//   accesorios) a través de un CanSink.
// - Escucha todas las tramas del bus (también las de la MS2) y mantiene una
//   imagen del estado: locos, sensores, accesorios, equipos conectados.
//
// No depende de Arduino: el ESP32 implementa CanSink con el controlador
// TWAI y los tests del PC con un objeto simulado.
#pragma once

#include <stdint.h>

#include <functional>

#include "marklin_can.h"

namespace marklin {

class CanSink {
 public:
  virtual ~CanSink() = default;
  virtual bool send(const CanFrame& frame) = 0;
};

enum class PowerState : uint8_t { Unknown, On, Off };

struct LocoState {
  uint32_t uid = 0;
  uint16_t speed = 0;      // 0..1000
  uint8_t direction = 0;   // 0 = desconocido, 1 = adelante, 2 = atrás
  uint32_t functions = 0;  // bit n = función Fn
  uint32_t lastChangeMs = 0;
  bool pinned = false;     // definida en la configuración (no se recicla)
  char name[20] = {0};
};

struct SensorState {
  uint32_t id = 0;
  int8_t state = -1;       // -1 desconocido, 0 libre, 1 ocupado/activo
  uint32_t onCount = 0;    // número de flancos libre->activo
  uint32_t offCount = 0;   // número de flancos activo->libre
  uint32_t changedMs = 0;
};

struct AccessoryState {
  uint32_t uid = 0;
  uint8_t position = 0;
  uint32_t changedMs = 0;
};

struct DeviceInfo {
  uint32_t uid = 0;
  uint16_t type = 0;
  uint8_t versionHigh = 0;
  uint8_t versionLow = 0;
  uint32_t lastSeenMs = 0;
};

class Controller {
 public:
  static constexpr int kMaxLocos = 48;
  static constexpr int kMaxSensors = 128;
  static constexpr int kMaxAccessories = 64;
  static constexpr int kMaxDevices = 8;
  static constexpr int kMaxPendingAccessory = 16;

  explicit Controller(CanSink* sink) : sink_(sink) {}

  // UID propio en el bus; de él se deriva el hash del identificador CAN.
  void setUid(uint32_t uid);
  uint32_t uid() const { return uid_; }
  uint16_t hash() const { return hash_; }

  // Llamar periódicamente (apaga bobinas de accesorios, etc.).
  void tick(uint32_t nowMs);
  uint32_t now() const { return now_; }

  // Procesa una trama recibida del bus.
  void handleFrame(const CanFrame& frame);

  // ---- Órdenes -------------------------------------------------------------
  bool setTrackPower(bool on);
  bool queryTrackPower();
  bool setSpeed(uint32_t locoUid, uint16_t speed);
  // Para la loco (velocidad 0) antes de cambiar el sentido, de modo que el
  // estado queda siempre definido: tras setDirection la velocidad es 0.
  bool setDirection(uint32_t locoUid, Direction dir);
  bool setFunction(uint32_t locoUid, uint8_t function, bool on);
  bool emergencyStop(uint32_t locoUid);
  // Pone a 0 la velocidad de todas las locos conocidas que estén en marcha.
  int stopAllLocos();
  // Activa la salida del accesorio y la desactiva tras `pulseMs`.
  bool switchAccessory(uint32_t accUid, uint8_t position, uint16_t pulseMs);
  bool ping();
  bool queryLoco(uint32_t locoUid);
  // Despierta la Gleisbox (si no hay MS2) y habilita los protocolos de vía.
  bool startGleisbox(uint8_t protocolMask);
  // Sensores locales (GPIO) o simulados.
  void setSensor(uint32_t id, bool active);

  // ---- Estado --------------------------------------------------------------
  PowerState power() const { return power_; }
  bool overload() const { return overload_; }
  const LocoState* findLoco(uint32_t uid) const;
  LocoState* touchLoco(uint32_t uid);  // la crea si no existe
  void setLocoName(uint32_t uid, const char* name);
  int locoCount() const { return locoCount_; }
  const LocoState& loco(int i) const { return locos_[i]; }

  const SensorState* findSensor(uint32_t id) const;
  int8_t sensorState(uint32_t id) const;
  int sensorCount() const { return sensorCount_; }
  const SensorState& sensor(int i) const { return sensors_[i]; }
  // Registra un sensor (aunque aún no tenga estado) para que aparezca en la UI.
  void declareSensor(uint32_t id);

  int accessoryCount() const { return accessoryCount_; }
  const AccessoryState& accessory(int i) const { return accessories_[i]; }

  int deviceCount() const { return deviceCount_; }
  const DeviceInfo& device(int i) const { return devices_[i]; }
  bool ms2Present() const;

  uint32_t framesRx() const { return framesRx_; }
  uint32_t framesTx() const { return framesTx_; }
  uint32_t txErrors() const { return txErrors_; }
  // Peticiones (no respuestas) recibidas de otros equipos: indica que hay
  // otro mando activo en el bus (MS2, CS2...).
  uint32_t foreignRequests() const { return foreignRequests_; }

  // Observador de todas las tramas (tx = true si la enviamos nosotros).
  std::function<void(const CanFrame&, bool tx)> onFrame;

 private:
  bool send(const CanFrame& f);
  SensorState* touchSensor(uint32_t id);
  void updateSensor(uint32_t id, bool active);
  void updateAccessory(uint32_t uid, uint8_t position);
  void updateDevice(const CanFrame& f);
  void applyLocoSpeed(uint32_t uid, uint16_t speed);
  void applyLocoDirection(uint32_t uid, uint8_t dir);
  void applyLocoFunction(uint32_t uid, uint8_t fn, uint8_t value);

  struct PendingAccessory {
    bool active = false;
    uint32_t uid = 0;
    uint8_t position = 0;
    uint32_t offAtMs = 0;
  };

  CanSink* sink_;
  uint32_t uid_ = 0x4D4B0001;  // "MK" + 1
  uint16_t hash_ = hashFromUid(0x4D4B0001);
  uint32_t now_ = 0;
  PowerState power_ = PowerState::Unknown;
  bool overload_ = false;

  LocoState locos_[kMaxLocos];
  int locoCount_ = 0;
  SensorState sensors_[kMaxSensors];
  int sensorCount_ = 0;
  AccessoryState accessories_[kMaxAccessories];
  int accessoryCount_ = 0;
  DeviceInfo devices_[kMaxDevices];
  int deviceCount_ = 0;
  PendingAccessory pending_[kMaxPendingAccessory];

  uint32_t framesRx_ = 0;
  uint32_t framesTx_ = 0;
  uint32_t txErrors_ = 0;
  uint32_t foreignRequests_ = 0;
};

}  // namespace marklin
