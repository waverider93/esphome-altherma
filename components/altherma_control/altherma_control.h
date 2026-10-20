#pragma once
// Heat deficit controller on the ESP: switches the heat request relay from values of the heat pump only.
// Runs without network; Home Assistant can only watch and change the settings.

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "deficit_model.h"

namespace esphome {
namespace altherma_control {

enum class Setting : uint8_t { START, STOP, MIN_RUN, MIN_PAUSE, ROOM_GAIN, UA };

class AlthermaControl : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  void set_room(sensor::Sensor *s) { this->room_ = s; }
  void set_setpoint(sensor::Sensor *s) { this->setpoint_ = s; }
  void set_outside(sensor::Sensor *s) { this->outside_ = s; }
  void set_flow(sensor::Sensor *s) { this->flow_ = s; }
  void set_leaving(sensor::Sensor *s) { this->leaving_ = s; }
  void set_inlet(sensor::Sensor *s) { this->inlet_ = s; }
#ifdef USE_BINARY_SENSOR
  void set_hot_water(binary_sensor::BinarySensor *s) { this->hot_water_ = s; }
#endif
  void set_heat_request(switch_::Switch *s) { this->heat_request_ = s; }
  void set_cool_request(switch_::Switch *s) { this->cool_request_ = s; }
  void set_default_ua(float ua) { this->default_ua_ = ua; }
  void set_room_limits(float low, float high) {
    this->model_.params.room_low_k = low;
    this->model_.params.room_high_k = high;
  }
  void set_calibration_hours(float h) { this->model_.params.calibration_h = h; }

  // Off: the model keeps running (shadow), but the relay is left alone
  void set_active(bool active);
  bool is_active() const { return this->active_; }

  // Settings, from the number entities
  float get_setting(Setting s) const;
  void set_setting(Setting s, float value);
#ifdef USE_NUMBER
  void add_setting_number(Setting s, number::Number *n) { this->numbers_.push_back({s, n}); }
#endif

  void set_deficit_sensor(sensor::Sensor *s) { this->deficit_sensor_ = s; }
  void set_ua_sensor(sensor::Sensor *s) { this->ua_sensor_ = s; }
  void set_loss_sensor(sensor::Sensor *s) { this->loss_sensor_ = s; }
  void set_heat_sensor(sensor::Sensor *s) { this->heat_sensor_ = s; }
  void set_calibration_sensor(sensor::Sensor *s) { this->calibration_sensor_ = s; }
#ifdef USE_TEXT_SENSOR
  void set_reason_sensor(text_sensor::TextSensor *s) { this->reason_sensor_ = s; }
#endif

 protected:
  struct SettingsStore {
    uint32_t version{1};
    float start, stop, min_run, min_pause, room_gain;
  };
  static float value_(sensor::Sensor *s) { return s != nullptr && s->has_state() ? s->state : NAN; }
  void save_settings_();
  void publish_settings_();

  DeficitModel model_;
  float default_ua_{90.0f};
  bool active_{false};
  bool last_running_{false};
  uint32_t changed_at_{0};
  uint32_t last_step_{0};
  uint32_t last_save_{0};
  ESPPreferenceObject state_pref_;
  ESPPreferenceObject settings_pref_;

  sensor::Sensor *room_{nullptr}, *setpoint_{nullptr}, *outside_{nullptr};
  sensor::Sensor *flow_{nullptr}, *leaving_{nullptr}, *inlet_{nullptr};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *hot_water_{nullptr};
#endif
  switch_::Switch *heat_request_{nullptr};
  switch_::Switch *cool_request_{nullptr};
  sensor::Sensor *deficit_sensor_{nullptr}, *ua_sensor_{nullptr}, *loss_sensor_{nullptr}, *heat_sensor_{nullptr},
      *calibration_sensor_{nullptr};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *reason_sensor_{nullptr};
#endif
#ifdef USE_NUMBER
  struct SettingNumber {
    Setting setting;
    number::Number *number;
  };
  std::vector<SettingNumber> numbers_;
#endif
};

#ifdef USE_NUMBER
class SettingNumber : public number::Number {
 public:
  SettingNumber(AlthermaControl *parent, Setting setting) : parent_(parent), setting_(setting) {}

 protected:
  void control(float value) override { this->parent_->set_setting(this->setting_, value); }
  AlthermaControl *parent_;
  Setting setting_;
};
#endif

}  // namespace altherma_control
}  // namespace esphome
