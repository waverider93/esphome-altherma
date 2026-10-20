#pragma once
// Reads a Daikin Altherma over its X10A serial port.
// Protocol, model detection and layout correction after ESPAltherma (MIT License, Copyright (c) 2020 Raomin).

#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
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
#ifdef USE_DAIKIN_X10A_DIAGRAM
#include "esphome/components/web_server_base/web_server_base.h"
#endif

#include "converters.h"

namespace esphome {
namespace daikin_x10a {

// One value inside a register reply. A value given by its label is placed on the ESP, from the catalog of the model
// it reads (configured or detected, with layout corrections); a raw value keeps its position.
struct Label {
  const char *label{nullptr};  // English catalog label, nullptr for a raw value
  int16_t reg_filter{-1};      // register the label must be in, -1: any
  int16_t entry{-1};           // catalog entry, -1 for a raw value
  bool active{false};
  uint8_t registry{0};
  uint8_t offset{0};
  uint16_t conv{0};
  uint8_t size{0};
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

// Survey, detection and layout check state; defined in daikin_x10a.cpp with the ESPAltherma headers
struct Engine;

class DaikinX10A : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_model(const char *model) { this->model_name_ = model; }
  void set_language(const char *language) { this->language_name_ = language; }
  void set_detect(bool detect) { this->detect_ = detect; }
  void set_layout_check(bool check) { this->layout_check_ = check; }
  void set_protocol(char protocol) { this->protocol_ = protocol; }
  void set_refrigerant(uint16_t r) { this->refrigerant_ = r; }
  void set_log_raw(bool log_raw) { this->log_raw_ = log_raw; }
#ifdef USE_SENSOR
  void add_sensor(sensor::Sensor *s, const char *label, int reg_filter, uint8_t reg, uint8_t offset, uint16_t conv,
                  uint8_t size);
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(binary_sensor::BinarySensor *s, const char *label, int reg_filter, uint8_t reg,
                         uint8_t offset, uint16_t conv, uint8_t size);
#endif
#ifdef USE_TEXT_SENSOR
  void add_text_sensor(text_sensor::TextSensor *s, const char *label, int reg_filter, uint8_t reg, uint8_t offset,
                       uint16_t conv, uint8_t size);
  void set_model_sensor(text_sensor::TextSensor *s) { this->model_sensor_ = s; }
  void set_detection_sensor(text_sensor::TextSensor *s) { this->detection_sensor_ = s; }
  void set_layout_sensor(text_sensor::TextSensor *s) { this->layout_sensor_ = s; }
#endif
#ifdef USE_DAIKIN_X10A_DIAGRAM
  void set_web_server_base(web_server_base::WebServerBase *base) { this->base_ = base; }
#endif

  // True while the last complete poll cycle answered within three update intervals
  bool is_connected() const;
  // Name of the model the values are read with, empty while it is not known
  const char *model_name() const;
  // Values for the diagram page as JSON; safe to call from the web server task
  std::string diagram_json();

  // One blocking query for the survey; fills buffer (64 bytes) with the reply frame
  bool query_blocking(uint8_t reg, uint8_t *buffer, char protocol);

 protected:
  enum class State : uint8_t { IDLE, SEND, WAIT };

  Label &add_label_(const char *label, int reg_filter, uint8_t reg, uint8_t offset, uint16_t conv, uint8_t size);
  size_t build_query_(uint8_t reg, char protocol, uint8_t *frame) const;
  void send_query_();
  void finish_reply_();
  void fail_reply_(const char *reason);
  void next_register_();
  void publish_(const uint8_t *payload, uint8_t len, uint8_t reg);
  size_t reply_length_(uint8_t reg, char protocol) const;

  void run_survey_();
  void apply_model_();
  bool safe_entry_(int entry) const;
  void check_layout_();
  void load_fixes_();
  void save_fixes_();
  void publish_info_();
  void build_diagram_json_();

  const char *model_name_{""};
  const char *language_name_{"en"};
  bool detect_{true};
  bool layout_check_{true};
  char protocol_{'I'};
  uint16_t refrigerant_{0};  // 0: from the model
  bool log_raw_{false};

  Engine *engine_{nullptr};
  int model_{-1};
  int language_{-1};
  bool survey_pending_{false};
  uint32_t survey_at_{0};
  ESPPreferenceObject fix_pref_;

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
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *model_sensor_{nullptr};
  text_sensor::TextSensor *detection_sensor_{nullptr};
  text_sensor::TextSensor *layout_sensor_{nullptr};
#endif
#ifdef USE_DAIKIN_X10A_DIAGRAM
  web_server_base::WebServerBase *base_{nullptr};
#endif
  Mutex json_lock_;
  std::string json_{"{}"};
};

}  // namespace daikin_x10a
}  // namespace esphome
