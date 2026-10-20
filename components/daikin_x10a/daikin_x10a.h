#pragma once
// Reads a Daikin Altherma over its X10A serial port.
// Protocol after ESPAltherma (include/comm.h, MIT License, Copyright (c) 2020 Raomin).

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "converters.h"

namespace esphome {
namespace daikin_x10a {

// One value inside a register reply, as in the ESPAltherma definition files
struct Label {
  uint8_t registry;
  uint8_t offset;
  uint16_t conv;
  uint8_t size;
#ifdef USE_SENSOR
  sensor::Sensor *sensor{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *binary_sensor{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *text_sensor{nullptr};
#endif
};

class DaikinX10A : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_protocol(char protocol) { this->protocol_ = protocol; }
  void set_refrigerant(uint16_t r) { this->converter_.refrigerant = (Refrigerant) r; }
  void set_log_raw(bool log_raw) { this->log_raw_ = log_raw; }
#ifdef USE_SENSOR
  void add_sensor(sensor::Sensor *s, uint8_t reg, uint8_t offset, uint16_t conv, uint8_t size);
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(binary_sensor::BinarySensor *s, uint8_t reg, uint8_t offset, uint16_t conv, uint8_t size);
  void set_connected_sensor(binary_sensor::BinarySensor *s) { this->connected_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void add_text_sensor(text_sensor::TextSensor *s, uint8_t reg, uint8_t offset, uint16_t conv, uint8_t size);
#endif

  // True while the last complete poll cycle answered within three update intervals
  bool is_connected() const;

 protected:
  enum class State : uint8_t { IDLE, SEND, WAIT };

  Label &add_label_(uint8_t reg, uint8_t offset, uint16_t conv, uint8_t size);
  void send_query_();
  void finish_reply_();
  void fail_reply_(const char *reason);
  void next_register_();
  void publish_(const uint8_t *payload, uint8_t len, uint8_t reg);
  size_t reply_length_(uint8_t reg) const;

  char protocol_{'I'};
  bool log_raw_{false};
  Converter converter_;
  std::vector<Label> labels_;
  std::vector<uint8_t> registries_;
  size_t current_{0};
  State state_{State::IDLE};
  uint32_t sent_at_{0};
  uint8_t buffer_[64];
  size_t received_{0};
  size_t expected_{0};
  bool cycle_ok_{false};
  uint32_t last_success_{0};
  bool ever_succeeded_{false};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connected_sensor_{nullptr};
#endif
};

}  // namespace daikin_x10a
}  // namespace esphome
