// Host test of the deficit controller: g++ -std=c++17 -Icomponents/altherma_control tests/test_deficit_model.cpp
#include <cassert>
#include <cmath>
#include <cstdio>

#include "deficit_model.h"

using namespace esphome::altherma_control;

// A simple house: UA_true W/K, C kWh/K, heated by the heat pump at power kW while the request is on
struct House {
  float ua, c, room, outside;
  float gains_kw{0};  // appliances, people: heat the controller does not see
};

static int simulate(DeficitModel &m, House &h, float power, int hours, int &starts, float &min_room, float &max_room) {
  bool running = false;
  float on_h = 10;
  starts = 0;
  min_room = 99;
  max_room = -99;
  const float dt = 1.0f / 60;
  for (int i = 0; i < hours * 60; i++) {
    Inputs in;
    in.room = h.room;
    in.setpoint = 21.5f;
    in.outside = h.outside;
    in.flow_l_min = running ? 7.0f : 0.0f;
    float heat = running ? power : 0;
    in.inlet = 24.0f;
    in.leaving = in.inlet + (running ? heat * 60 / (7.0f * 4.18f) : 0);
    Decision d = m.step(in, dt, running, on_h);
    if (d == Decision::START && !running) {
      running = true;
      on_h = 0;
      starts++;
    } else if (d == Decision::STOP && running) {
      running = false;
      on_h = 0;
    }
    on_h += dt;
    h.room += (heat + h.gains_kw - h.ua * (h.room - h.outside) / 1000) * dt / h.c;
    if (i > 24 * 60) {  // after settling
      min_room = std::fmin(min_room, h.room);
      max_room = std::fmax(max_room, h.room);
    }
  }
  return 0;
}

int main() {
  // Few long runs at 5 °C outside, room held near the setpoint
  {
    DeficitModel m;
    m.state.ua_w_k = 90;
    House h{90, 13, 21.5f, 5};
    int starts;
    float lo, hi;
    simulate(m, h, 2.5f, 72, starts, lo, hi);
    printf("5 C: %d starts in 72 h, room %.2f..%.2f\n", starts, lo, hi);
    assert(starts > 0 && starts <= 3 * 8);
    assert(lo > 21.0f && hi < 22.5f);
  }
  // UA learns from a wrong start value
  {
    DeficitModel m;
    m.state.ua_w_k = 150;
    House h{90, 13, 21.5f, 2};
    int starts;
    float lo, hi;
    simulate(m, h, 3.0f, 24 * 30, starts, lo, hi);
    printf("UA learned: %.0f W/K (true 90) after %u calibrations\n", m.state.ua_w_k, (unsigned) m.state.calibrations);
    assert(m.state.calibrations >= 5);
    assert(std::fabs(m.state.ua_w_k - 90) < 15);
  }
  // With gains (the house of 20-24 Oct 2026: 93 W/K, 0.35 kW from the household, 10 C outside) UA settles on the
  // effective value, 93 - 350 / 11.5 = 63 W/K, from a start value that is too high
  {
    DeficitModel m;
    m.state.ua_w_k = 96;
    House h{93, 14, 21.5f, 10, 0.35f};
    int starts;
    float lo, hi;
    simulate(m, h, 1.5f, 24 * 21, starts, lo, hi);
    printf("UA with gains: %.0f W/K (effective 63) after %u calibrations, %d starts in 21 days, room %.2f..%.2f\n",
           m.state.ua_w_k, (unsigned) m.state.calibrations, starts, lo, hi);
    assert(m.state.calibrations >= 5);
    assert(std::fabs(m.state.ua_w_k - 63) < 6);
  }
  // No values: no switching decision
  {
    DeficitModel m;
    Inputs in;
    assert(m.step(in, 1.0f / 60, false, 5) == Decision::NO_DATA);
  }
  // Minimum run time holds, the room limit overrides it
  {
    DeficitModel m;
    Inputs in;
    in.room = 21.5f;
    in.setpoint = 21.5f;
    in.outside = 10;
    m.state.deficit_kwh = -1;
    assert(m.step(in, 1.0f / 60, true, 0.5f) == Decision::KEEP);
    in.room = 22.6f;
    m.state.deficit_kwh = 2;
    assert(m.step(in, 1.0f / 60, true, 0.5f) == Decision::STOP);
    // the open deficit is dropped: the warm room says there is none
    assert(m.state.deficit_kwh < 0);
  }
  // The heat pump stopped itself: the request ends too, unless the house still needs a lot
  {
    DeficitModel m;
    Inputs in;
    in.room = 21.5f;
    in.setpoint = 21.5f;
    in.outside = 10;
    m.state.deficit_kwh = 1;
    assert(m.step(in, 1.0f / 60, true, 0.0f, true) == Decision::STOP);
    m.state.deficit_kwh = 4;
    m.state.last_setpoint = NAN;
    assert(m.step(in, 1.0f / 60, true, 0.0f, true) == Decision::KEEP);
  }
  printf("deficit model ok\n");
  return 0;
}
