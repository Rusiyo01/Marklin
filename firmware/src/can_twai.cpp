#include "can_twai.h"

#include <driver/twai.h>
#include <string.h>

bool TwaiBus::begin(int txPin, int rxPin) {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
      static_cast<gpio_num_t>(txPin), static_cast<gpio_num_t>(rxPin), TWAI_MODE_NORMAL);
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
  if (!started_ || nowMs - lastPollMs_ < 100) return;
  lastPollMs_ = nowMs;
  twai_status_info_t st;
  if (twai_get_status_info(&st) != ESP_OK) return;
  state_ = static_cast<uint8_t>(st.state);
  busErrors_ = st.bus_error_count;
  txErrorCounter_ = st.tx_error_counter;
  rxErrorCounter_ = st.rx_error_counter;
  if (st.state == TWAI_STATE_BUS_OFF) {
    // Demasiados errores (p.ej. cable suelto): reiniciar el controlador.
    twai_initiate_recovery();
    recoveries_++;
  } else if (st.state == TWAI_STATE_STOPPED) {
    twai_start();
  }
}

const char* TwaiBus::stateName() const {
  if (!started_) return "not started";
  switch (state_) {
    case TWAI_STATE_STOPPED: return "parado";
    case TWAI_STATE_RUNNING: return "ok";
    case TWAI_STATE_BUS_OFF: return "bus-off";
    case TWAI_STATE_RECOVERING: return "recuperando";
    default: return "?";
  }
}
