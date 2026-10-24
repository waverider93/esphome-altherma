#pragma once
// Heat deficit controller for a heat pump driven through its room thermostat input.
// Plain C++ without ESPHome, so it can be tested on the host (tests/test_deficit_model.cpp).
//
// Every step: deficit += (UA x (room - outside) - heat delivered) x dt, pulled slowly towards k x (setpoint - room).
// UA learns itself from the heat delivered over a few days ("effective" UA: average gains are inside), with the heat
// the floor stored or gave back over the window (k x room change) taken out. The window slides: every day it looks
// back over the last three days.

#include <cmath>
#include <cstdint>

namespace esphome {
namespace altherma_control {

struct Params {
  float start_kwh{3.0f};      // start at this deficit
  float stop_kwh{0.0f};       // stop at this deficit (after the minimum run time)
  float min_run_h{2.0f};
  float min_pause_h{1.0f};
  float room_gain_kwh_k{14.0f};  // k: heat the floor stores per kelvin of room; deficit target per kelvin below setpoint
  float anchor_h{12.0f};        // time constant of the pull towards the target
  float room_low_k{0.5f};       // room below setpoint - this: on at once
  float room_high_k{1.0f};      // room above setpoint + this: off at once
  float calibration_h{72.0f};   // length of a calibration window; it moves on by a third of this
};

// Everything that must survive a reboot
struct State {
  uint32_t version{3};
  float ua_w_k{60.0f};
  float deficit_kwh{0.0f};
  float last_setpoint{NAN};
  // calibration window: the running third, and the last three finished thirds (index 0 the newest)
  float window_h{0.0f};
  float window_heat_kwh{0.0f};
  float window_kh{0.0f};  // sum of (room - outside) x hours
  float window_room_start{NAN};
  float window_invalid_h{0.0f};
  uint32_t parts{0};  // finished thirds kept, up to 3
  float part_h[3]{};
  float part_heat_kwh[3]{};
  float part_kh[3]{};
  float part_room_start[3]{NAN, NAN, NAN};
  float part_invalid_h[3]{};
  uint32_t calibrations{0};
};

struct Inputs {
  float room{NAN};
  float setpoint{NAN};
  float outside{NAN};
  float flow_l_min{NAN};
  float leaving{NAN};
  float inlet{NAN};
  bool hot_water{false};  // 3-way valve on the tank: heat goes there, not to the house
};

enum class Decision : uint8_t { KEEP, START, STOP, NO_DATA };

class DeficitModel {
 public:
  Params params;
  State state;
  float loss_kw{NAN};
  float heat_kw{NAN};
  const char *reason{""};

  static float heat_power_kw(const Inputs &in) {
    if (in.hot_water || std::isnan(in.flow_l_min) || std::isnan(in.leaving) || std::isnan(in.inlet))
      return 0.0f;
    // water: 4.18 kJ/(kg K), 1 l = 1 kg; l/min -> kg/s
    return in.flow_l_min / 60.0f * 4.18f * (in.leaving - in.inlet);
  }

  // One step of dt_h hours. running: the heat request is on; on_h: hours since it last changed (or, with the
  // compressor known, hours of compressor run or pause). stopped_itself: the heat pump stopped its compressor on its
  // own during the request (flow temperature limit [9-04]) and has stayed off for a few minutes.
  Decision step(const Inputs &in, float dt_h, bool running, float on_h, bool stopped_itself = false) {
    if (std::isnan(in.room) || std::isnan(in.outside) || std::isnan(in.setpoint)) {
      this->reason = "no room, setpoint or outside temperature";
      this->state.window_invalid_h += dt_h;
      this->state.window_h += dt_h;
      return Decision::NO_DATA;
    }
    State &s = this->state;
    this->loss_kw = s.ua_w_k * (in.room - in.outside) / 1000.0f;
    if (this->loss_kw < 0)
      this->loss_kw = 0;
    this->heat_kw = heat_power_kw(in);
    s.deficit_kwh += (this->loss_kw - this->heat_kw) * dt_h;

    // A new setpoint moves the deficit at once by what the house needs for it
    if (!std::isnan(s.last_setpoint) && in.setpoint != s.last_setpoint)
      s.deficit_kwh += this->params.room_gain_kwh_k * (in.setpoint - s.last_setpoint);
    s.last_setpoint = in.setpoint;

    // Room anchor: model errors do not add up
    float target = this->params.room_gain_kwh_k * (in.setpoint - in.room);
    if (this->params.anchor_h > 0)
      s.deficit_kwh += (target - s.deficit_kwh) * std::fmin(1.0f, dt_h / this->params.anchor_h);
    // Outside the room limits the room is right and the model is not: the deficit jumps to what the room says,
    // so it does not start again right after a stop for a warm room (or stop right after a start for a cold one)
    if (in.room > in.setpoint + this->params.room_high_k)
      s.deficit_kwh = std::fmin(s.deficit_kwh, target);
    else if (in.room < in.setpoint - this->params.room_low_k)
      s.deficit_kwh = std::fmax(s.deficit_kwh, target);
    s.deficit_kwh = std::fmax(-10.0f, std::fmin(15.0f, s.deficit_kwh));

    this->calibrate_(in, dt_h);

    const Params &p = this->params;
    if (running) {
      if (in.room > in.setpoint + p.room_high_k) {
        this->reason = "room above setpoint";
        return Decision::STOP;
      }
      if (s.deficit_kwh <= p.stop_kwh && on_h >= p.min_run_h) {
        this->reason = "deficit covered";
        return Decision::STOP;
      }
      // It stopped itself anyway: end the request too, so the pause starts now instead of the heat pump starting
      // again on its own a few minutes later. Not when the house still clearly needs heat.
      if (stopped_itself && s.deficit_kwh < p.start_kwh) {
        this->reason = "heat pump stopped itself";
        return Decision::STOP;
      }
      this->reason = on_h < p.min_run_h ? "minimum run time" : "deficit not covered";
      return Decision::KEEP;
    }
    if (in.room < in.setpoint - p.room_low_k) {
      this->reason = "room below setpoint";
      return Decision::START;
    }
    if (s.deficit_kwh >= p.start_kwh && on_h >= p.min_pause_h) {
      this->reason = "deficit reached";
      return Decision::START;
    }
    this->reason = on_h < p.min_pause_h ? "minimum pause" : "no demand";
    return Decision::KEEP;
  }

  // Hours to the next calibration
  float calibration_left_h() const {
    return std::fmax(0.0f, this->params.calibration_h / 3 - this->state.window_h);
  }

  // Start over: a fresh window (after UA was set by hand)
  void clear_window() {
    State &s = this->state;
    s.window_h = s.window_kh = s.window_heat_kwh = s.window_invalid_h = 0;
    s.window_room_start = NAN;
    s.parts = 0;
  }

 protected:
  void calibrate_(const Inputs &in, float dt_h) {
    State &s = this->state;
    if (std::isnan(s.window_room_start))
      s.window_room_start = in.room;
    s.window_h += dt_h;
    s.window_kh += (in.room - in.outside) * dt_h;
    s.window_heat_kwh += this->heat_kw * dt_h;
    if (s.window_h < this->params.calibration_h / 3)
      return;
    // This third is finished: keep it with the two before
    for (int i = 2; i > 0; i--) {
      s.part_h[i] = s.part_h[i - 1];
      s.part_heat_kwh[i] = s.part_heat_kwh[i - 1];
      s.part_kh[i] = s.part_kh[i - 1];
      s.part_room_start[i] = s.part_room_start[i - 1];
      s.part_invalid_h[i] = s.part_invalid_h[i - 1];
    }
    s.part_h[0] = s.window_h;
    s.part_heat_kwh[0] = s.window_heat_kwh;
    s.part_kh[0] = s.window_kh;
    s.part_room_start[0] = s.window_room_start;
    s.part_invalid_h[0] = s.window_invalid_h;
    if (s.parts < 3)
      s.parts++;
    s.window_h = 0;
    s.window_kh = 0;
    s.window_heat_kwh = 0;
    s.window_invalid_h = 0;
    s.window_room_start = in.room;
    if (s.parts < 3)
      return;
    float h = s.part_h[0] + s.part_h[1] + s.part_h[2];
    float heat = s.part_heat_kwh[0] + s.part_heat_kwh[1] + s.part_heat_kwh[2];
    float kh = s.part_kh[0] + s.part_kh[1] + s.part_kh[2];
    float invalid = s.part_invalid_h[0] + s.part_invalid_h[1] + s.part_invalid_h[2];
    // Every window counts: the heat the floor stored (room warmer at the end) or gave back is taken out with k.
    // Only when the data was complete, the room moved less than 2 K, and it was cold enough outside for the numbers
    // to mean something.
    float room_change = in.room - s.part_room_start[2];
    bool settled = std::fabs(room_change) < 2.0f;
    bool complete = invalid < 0.1f * h;
    bool cold_enough = kh > 5.0f * h;  // on average at least 5 K between room and outside
    if (settled && complete && cold_enough && heat > 0) {
      float measured = (heat - this->params.room_gain_kwh_k * room_change) * 1000.0f / kh;
      if (measured > 20.0f && measured < 400.0f) {
        // Windows a day apart share two thirds of their data, so each moves UA by a third only
        s.ua_w_k = s.ua_w_k * 2 / 3 + measured / 3;
        s.calibrations++;
      }
    }
  }
};

}  // namespace altherma_control
}  // namespace esphome
