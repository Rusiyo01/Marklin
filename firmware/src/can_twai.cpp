#include "can_twai.h"

#include <driver/twai.h>
#include <string.h>

bool TwaiBus::begin(int txPin, int rxPin) {
  txPin_ = txPin;
  rxPin_ = rxPin;
  return install();
}

bool TwaiBus::install() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      static_cast<gpio_num_t>(txPin_), static_cast<gpio_num_t>(rxPin_), TWAI_MODE_NORMAL);
  g.tx_queue_len = 32;
  g.rx_queue_len = 64;
  // Märklin: 250 kbit/s.
  twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }
  started_ = true;
  state_ = TWAI_STATE_RUNNING;
  return true;
}

bool TwaiBus::send(const marklin::CanFrame& frame) {
  if (!started_) return false;
  twai_message_t m;
  memset(&m, 0, sizeof(m));
  m.identifier = frame.id & 0x1FFFFFFF;
  m.flags = TWAI_MSG_FLAG_EXTD;
  m.data_length_code = frame.dlc > 8 ? 8 : frame.dlc;
  memcpy(m.data, frame.data, m.data_length_code);
  return twai_transmit(&m, pdMS_TO_TICKS(5)) == ESP_OK;
}

bool TwaiBus::receive(marklin::CanFrame* frame) {
  if (!started_) return false;
  twai_message_t m;
  while (twai_receive(&m, 0) == ESP_OK) {
    if (!(m.flags & TWAI_MSG_FLAG_EXTD) || (m.flags & TWAI_MSG_FLAG_RTR)) continue;
    frame->id = m.identifier;
    frame->dlc = m.data_length_code > 8 ? 8 : m.data_length_code;
    memset(frame->data, 0, sizeof(frame->data));
    memcpy(frame->data, m.data, frame->dlc);
    return true;
  }
  return false;
}

void TwaiBus::poll(uint32_t nowMs) {
  if (reinstallAtMs_ && static_cast<int32_t>(nowMs - reinstallAtMs_) >= 0) {
    reinstallAtMs_ = 0;
    if (install()) recoveries_++;
    else reinstallAtMs_ = nowMs + kReinstallDelayMs;
    return;
  }
  if (!started_ || nowMs - lastPollMs_ < 100) return;
  lastPollMs_ = nowMs;
  twai_status_info_t st;
  if (twai_get_status_info(&st) != ESP_OK) return;
  state_ = static_cast<uint8_t>(st.state);
  busErrors_ = st.bus_error_count;
  txErrorCounter_ = st.tx_error_counter;
  rxErrorCounter_ = st.rx_error_counter;
  if (st.state == TWAI_STATE_BUS_OFF) {
    // Demasiados errores (p.ej. cable suelto o Gleisbox apagada): reinstalar
    // el controlador al cabo de un rato. No se usa twai_initiate_recovery():
    // en el driver de ESP-IDF 4.4 pone a cero la cuenta de tramas pendientes
    // y, si luego llega la interrupción de la trama abortada, salta el
    // assert "tx_msg_count >= 0" y el ESP32 se reinicia sin parar.
    // Desinstalar libera la interrupción y resetea el periférico.
    if (twai_driver_uninstall() == ESP_OK) {
      started_ = false;
      reinstallAtMs_ = nowMs + kReinstallDelayMs;
      if (reinstallAtMs_ == 0) reinstallAtMs_ = 1;
    }
  }
}

const char* TwaiBus::stateName() const {
  if (reinstallAtMs_) return "bus-off";
  if (!started_) return "not started";
  switch (state_) {
    case TWAI_STATE_STOPPED: return "stopped";
    case TWAI_STATE_RUNNING: return "ok";
    case TWAI_STATE_BUS_OFF: return "bus-off";
    case TWAI_STATE_RECOVERING: return "recovering";
    default: return "?";
  }
}
