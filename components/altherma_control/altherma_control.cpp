#include "altherma_control.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace altherma_control {

static const char *const TAG = "altherma_control";
static const uint32_t SAVE_INTERVAL_MS = 10 * 60 * 1000;  // flash wear: the state is saved every 10 minutes

void AlthermaControl::setup() {
  this->state_pref_ = global_preferences->make_preference<State>(fnv1_hash("altherma_control_state"));
  this->settings_pref_ = global_preferences->make_preference<SettingsStore>(fnv1_hash("altherma_control_settings"));
  State stored;
  if (this->state_pref_.load(&stored) && stored.version == State().version) {
    this->model_.state = stored;
    ESP_LOGI(TAG, "Restored: UA %.0f W/K (%u calibrations), deficit %.2f kWh", stored.ua_w_k,
             (unsigned) stored.calibrations, stored.deficit_kwh);
  } else {
    this->model_.state.ua_w_k = this->default_ua_;
  }
  SettingsStore settings;
  if (this->settings_pref_.load(&settings) && settings.version == SettingsStore().version) {
    Params &p = this->model_.params;
    p.start_kwh = settings.start;
    p.stop_kwh = settings.stop;
    p.min_run_h = settings.min_run;
    p.min_pause_h = settings.min_pause;
    p.room_gain_kwh_k = settings.room_gain;
  }
  // After a reboot neither the minimum run time nor the pause holds anything back
  float hold_h = std::fmax(this->model_.params.min_run_h, this->model_.params.min_pause_h);
  this->changed_at_ = millis() - (uint32_t) (hold_h * 3600000.0f);
  this->last_running_ = this->heat_request_ != nullptr && this->heat_request_->state;
  this->last_step_ = millis();
  this->last_save_ = millis();
  this->publish_settings_();
}

void AlthermaControl::dump_config() {
  const Params &p = this->model_.params;
  ESP_LOGCONFIG(TAG, "Altherma control:");
  ESP_LOGCONFIG(TAG, "  Active: %s", YESNO(this->active_));
  ESP_LOGCONFIG(TAG, "  Start %.1f kWh, stop %.1f kWh, minimum run %.1f h, minimum pause %.1f h", p.start_kwh,
                p.stop_kwh, p.min_run_h, p.min_pause_h);
  ESP_LOGCONFIG(TAG, "  UA %.0f W/K, room gain %.1f kWh/K, calibration window %.0f h", this->model_.state.ua_w_k,
                p.room_gain_kwh_k, p.calibration_h);
  LOG_UPDATE_INTERVAL(this);
}

void AlthermaControl::set_active(bool active) {
  if (active != this->active_)
    ESP_LOGI(TAG, "Control %s", active ? "active: the ESP switches the heat request" : "passive (model only)");
  this->active_ = active;
}

float AlthermaControl::get_setting(Setting s) const {
  const Params &p = this->model_.params;
  switch (s) {
    case Setting::START:
      return p.start_kwh;
    case Setting::STOP:
      return p.stop_kwh;
    case Setting::MIN_RUN:
      return p.min_run_h;
    case Setting::MIN_PAUSE:
      return p.min_pause_h;
    case Setting::ROOM_GAIN:
      return p.room_gain_kwh_k;
    case Setting::UA:
      return this->model_.state.ua_w_k;
  }
  return NAN;
}

void AlthermaControl::set_setting(Setting s, float value) {
  Params &p = this->model_.params;
  switch (s) {
    case Setting::START:
      p.start_kwh = value;
      break;
    case Setting::STOP:
      p.stop_kwh = value;
      break;
    case Setting::MIN_RUN:
      p.min_run_h = value;
      break;
    case Setting::MIN_PAUSE:
      p.min_pause_h = value;
      break;
    case Setting::ROOM_GAIN:
      p.room_gain_kwh_k = value;
      break;
    case Setting::UA:
      // Set by hand: the learning goes on from here, in a fresh window
      this->model_.state.ua_w_k = value;
      this->model_.state.window_h = 0;
      this->model_.state.window_kh = 0;
      this->model_.state.window_heat_kwh = 0;
      this->model_.state.window_invalid_h = 0;
      this->model_.state.window_room_start = NAN;
      this->state_pref_.save(&this->model_.state);
      break;
  }
  this->save_settings_();
  this->publish_settings_();
}

void AlthermaControl::save_settings_() {
  const Params &p = this->model_.params;
  SettingsStore store;
  store.start = p.start_kwh;
  store.stop = p.stop_kwh;
  store.min_run = p.min_run_h;
  store.min_pause = p.min_pause_h;
  store.room_gain = p.room_gain_kwh_k;
  this->settings_pref_.save(&store);
}

void AlthermaControl::publish_settings_() {
#ifdef USE_NUMBER
  for (auto &n : this->numbers_)
    n.number->publish_state(this->get_setting(n.setting));
#endif
}

std::string AlthermaControl::json() const {
  const Params &p = this->model_.params;
  const State &st = this->model_.state;
  auto num = [](float v) {
    char b[16];
    if (std::isnan(v))
      return std::string("null");
    snprintf(b, sizeof(b), "%.2f", v);
    return std::string(b);
  };
  std::string s = "{\"active\":";
  s += this->active_ ? "true" : "false";
  s += ",\"request\":";
  s += this->heat_request_ != nullptr && this->heat_request_->state ? "true" : "false";
  s += ",\"reason\":\"";
  s += this->model_.reason;  // fixed English texts without quotes
  s += "\",\"deficit\":" + num(st.deficit_kwh) + ",\"start\":" + num(p.start_kwh) + ",\"stop\":" + num(p.stop_kwh);
  s += ",\"min_run\":" + num(p.min_run_h) + ",\"min_pause\":" + num(p.min_pause_h);
  s += ",\"room_gain\":" + num(p.room_gain_kwh_k) + ",\"ua\":" + num(st.ua_w_k);
  s += ",\"calibrations\":" + std::to_string(st.calibrations);
  s += ",\"calibration_left\":" + num(this->model_.calibration_left_h());
  s += ",\"loss\":" + num(this->model_.loss_kw) + ",\"heat\":" + num(this->model_.heat_kw);
  s += ",\"room_low\":" + num(p.room_low_k) + ",\"room_high\":" + num(p.room_high_k) + "}";
  return s;
}

void AlthermaControl::update() {
  uint32_t now = millis();
  float dt_h = (now - this->last_step_) / 3600000.0f;
  this->last_step_ = now;
  if (dt_h <= 0 || dt_h > 1)
    dt_h = this->get_update_interval() / 3600000.0f;

  bool running = this->heat_request_ != nullptr && this->heat_request_->state;
  if (running != this->last_running_) {
    this->changed_at_ = now;  // switched by someone else (Home Assistant, by hand) counts too
    this->last_running_ = running;
  }
  float on_h = (now - this->changed_at_) / 3600000.0f;

  Inputs in;
  in.room = value_(this->room_);
  in.setpoint = value_(this->setpoint_);
  in.outside = value_(this->outside_);
  in.flow_l_min = value_(this->flow_);
  in.leaving = value_(this->leaving_);
  in.inlet = value_(this->inlet_);
#ifdef USE_BINARY_SENSOR
  in.hot_water = this->hot_water_ != nullptr && this->hot_water_->has_state() && this->hot_water_->state;
#endif
  float ua_before = this->model_.state.ua_w_k;
  Decision d = this->model_.step(in, dt_h, running, on_h);

  bool cooling = this->cool_request_ != nullptr && this->cool_request_->state;
  if (this->active_ && !cooling && this->heat_request_ != nullptr) {
    if (d == Decision::NO_DATA && !running) {
      ESP_LOGW(TAG, "No values from the heat pump: heat request on, it follows its heating curve");
      this->heat_request_->turn_on();
    } else if (d == Decision::START) {
      ESP_LOGI(TAG, "Heat request on: %s (deficit %.2f kWh)", this->model_.reason, this->model_.state.deficit_kwh);
      this->heat_request_->turn_on();
    } else if (d == Decision::STOP) {
      ESP_LOGI(TAG, "Heat request off: %s (deficit %.2f kWh)", this->model_.reason, this->model_.state.deficit_kwh);
      this->heat_request_->turn_off();
    }
  }

  if (this->deficit_sensor_ != nullptr)
    this->deficit_sensor_->publish_state(this->model_.state.deficit_kwh);
  if (this->ua_sensor_ != nullptr)
    this->ua_sensor_->publish_state(this->model_.state.ua_w_k);
  if (this->loss_sensor_ != nullptr)
    this->loss_sensor_->publish_state(this->model_.loss_kw);
  if (this->heat_sensor_ != nullptr)
    this->heat_sensor_->publish_state(this->model_.heat_kw);
  if (this->calibration_sensor_ != nullptr)
    this->calibration_sensor_->publish_state(this->model_.calibration_left_h());
#ifdef USE_TEXT_SENSOR
  if (this->reason_sensor_ != nullptr)
    this->reason_sensor_->publish_state(this->active_ ? this->model_.reason : "passive");
#endif

  bool calibrated = this->model_.state.ua_w_k != ua_before;
  if (calibrated) {
    ESP_LOGI(TAG, "Calibrated: UA %.0f W/K", this->model_.state.ua_w_k);
    this->publish_settings_();
  }
  if (calibrated || now - this->last_save_ > SAVE_INTERVAL_MS) {
    this->state_pref_.save(&this->model_.state);
    this->last_save_ = now;
  }
}

}  // namespace altherma_control
}  // namespace esphome
