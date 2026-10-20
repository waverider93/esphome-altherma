// Host test of the value conversions: g++ -std=c++17 -I../components/daikin_x10a test_converters.cpp && ./a.out
#include <cassert>
#include <cmath>
#include <cstdio>
#include "converters.h"

using namespace esphome::daikin_x10a;

int main() {
  Converter c;
  const uint8_t t23[] = {0xE6, 0x00};  // 230 * 0.1
  assert(std::fabs(c.convert(105, t23, 2).number - 23.0f) < 1e-4);
  const uint8_t neg[] = {0xF6, 0xFF};  // -10 * 0.1
  assert(std::fabs(c.convert(105, neg, 2).number + 1.0f) < 1e-4);
  const uint8_t bits[] = {0x02};
  assert(c.convert(301, bits, 1).state == true);
  assert(c.convert(300, bits, 1).state == false);
  const uint8_t mode[] = {0x10};
  assert(c.convert(315, mode, 1).text == "Heating");
  const uint8_t err[] = {0x00};
  assert(c.convert(204, err, 1).text == " 0");
  assert(c.convert(999, err, 1).kind == ValueKind::INVALID);
  puts("converters ok");
}
