// Acceso al bus CAN con el controlador TWAI integrado en el ESP32.
// Solo hace falta un transceptor externo (p.ej. SN65HVD230 a 3,3 V).
#pragma once

#include <Arduino.h>

#include "marklin_controller.h"

class TwaiBus : public marklin::CanSink {
 public:
  bool begin(int txPin, int rxPin);
  bool send(const marklin::CanFrame& frame) override;
  // Lectura no bloqueante. Ignora tramas estándar (11 bits) y RTR.
  bool receive(marklin::CanFrame* frame);
  // Vigila el estado del controlador y se recupera de "bus-off".
  void poll(uint32_t nowMs);

  bool started() const { return started_; }
  const char* stateName() const;
  uint32_t busErrors() const { return busErrors_; }
  uint32_t recoveries() const { return recoveries_; }
  // Contadores de error del controlador (TEC/REC). Si el TEC sube hasta
  // 128 o más, nadie confirma nuestras tramas: revisar cableado CAN-H/CAN-L.
  uint32_t txErrorCounter() const { return txErrorCounter_; }
  uint32_t rxErrorCounter() const { return rxErrorCounter_; }

 private:
  bool started_ = false;
  uint8_t state_ = 0;
  uint32_t busErrors_ = 0;
  uint32_t recoveries_ = 0;
  uint32_t txErrorCounter_ = 0;
  uint32_t rxErrorCounter_ = 0;
  uint32_t lastPollMs_ = 0;
};
