#pragma once
// Value conversions of the Daikin X10A protocol.
// Ported from ESPAltherma (include/converters.h, MIT License, Copyright (c) 2020 Raomin).

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace esphome {
namespace daikin_x10a {

enum class ValueKind : uint8_t { NUMBER, TEXT, BOOL, INVALID };

struct Value {
  ValueKind kind{ValueKind::INVALID};
  float number{NAN};
  bool state{false};
  std::string text;
};

enum class Refrigerant : uint16_t { NONE = 800, R410A = 801, R32 = 802, R22 = 803, R134A = 804 };

// True when the conversion yields a number, an on/off state or a text (used by the code generator too).
inline bool conv_is_bool(int conv) { return (conv >= 300 && conv <= 307) || conv == 200; }

class Converter {
 public:
  Refrigerant refrigerant{Refrigerant::R32};

  Value convert(int conv, const uint8_t *data, int size) {
    Value v;
    double d = NAN;
    switch (conv) {
      case 100: {
        v.kind = ValueKind::TEXT;
        v.text.assign(reinterpret_cast<const char *>(data), size > 0 ? size - 1 : 0);
        return v;
      }
      case 101: d = get_signed(data, size, 0); break;
      case 102: d = get_signed(data, size, 1); break;
      case 103: d = get_signed(data, size, 0) / 256.0; break;
      case 104: d = get_signed(data, size, 1) / 256.0; break;
      case 105: d = get_signed(data, size, 0) * 0.1; break;
      case 106: d = get_signed(data, size, 1) * 0.1; break;
      case 107:
        d = get_signed(data, size, 0) * 0.1;
        if (d == -3276.8)
          return text_("---");
        break;
      case 108:
        d = get_signed(data, size, 1) * 0.1;
        if (d == -3276.8)
          return text_("---");
        break;
      case 109: d = get_signed(data, size, 0) / 256.0 * 2.0; break;
      case 110: d = get_signed(data, size, 1) / 256.0 * 2.0; break;
      case 111: d = get_signed(data, size, 1) * 0.5; break;
      case 112: d = (get_signed(data, size, 1) - 64) * 0.5; break;
      case 113: d = get_signed(data, size, 1) * 0.25; break;
      case 114: {
        if (data[0] == 0 && data[1] == 128)
          return text_("---");
        uint16_t n = (uint16_t) (data[1] * 256);
        n |= data[0];
        if ((data[1] & 128) != 0)
          n = ~(n - 1);
        d = (double) ((n & 65280) / 256);
        d += (double) (n & 255) / 256.0;
        d *= 10.0;
        if ((data[1] & 128) != 0)
          d *= -1.0;
        break;
      }
      case 115: d = get_signed(data, size, 0) / 2560.0; break;
      case 116: d = get_signed(data, size, 1) / 2560.0; break;
      case 117: d = get_signed(data, size, 0) * 0.01; break;
      case 118: d = get_signed(data, size, 1) * 0.01; break;
      case 119:
        if (data[0] == 0 && data[1] == 128)
          return text_("---");
        d = get_signed(data, size, 0) / 256.0;
        break;
      case 151: d = get_unsigned(data, size, 0); break;
      case 152: d = get_unsigned(data, size, 1); break;
      case 153: d = get_unsigned(data, size, 0) / 256.0; break;
      case 154: d = get_unsigned(data, size, 1) / 256.0; break;
      case 155: d = get_unsigned(data, size, 0) * 0.1; break;
      case 156: d = get_unsigned(data, size, 1) * 0.1; break;
      case 157: d = get_unsigned(data, size, 0) / 256.0 * 2.0; break;
      case 158: d = get_unsigned(data, size, 1) / 256.0 * 2.0; break;
      case 161: d = get_unsigned(data, size, 1) * 0.5; break;
      case 162: d = (get_unsigned(data, size, 1) - 64) * 0.5; break;
      case 163: d = get_unsigned(data, size, 1) * 0.25; break;
      case 164: d = get_unsigned(data, size, 1) * 5; break;
      case 165: d = get_unsigned(data, size, 0) & 16383; break;
      case 200: return bool_(data[0] != 0);
      case 203: {
        static const char *const t[] = {"Normal", "Error", "Warning", "Caution"};
        return text_(data[0] < 4 ? t[data[0]] : "-");
      }
      case 204: {
        static const char a[] = " ACEHFJLPU987654";
        static const char b[] = "0123456789AHCJEF";
        char s[3] = {a[(data[0] >> 4) & 15], b[data[0] & 15], 0};
        return text_(s);
      }
      case 211:
        // ESPAltherma compares the pointer with 0 here, so it never prints OFF; keep the number
        d = data[0];
        break;
      case 212: {
        int c = data[0];
        if (c == 0)
          return text_("---");
        if (c >= 251)
          return text_(std::to_string(280 + (c - 251) * 10));
        return text_(std::to_string(c));
      }
      case 213: {
        int c = data[0];
        if (c >= 10 && c <= 15)
          return text_(std::string(1, (char) ('A' + c - 10)));
        return text_(std::to_string(c));
      }
      case 214: {
        int c = data[0];
        if (c == 0)
          return text_(" ");
        if (c >= 1 && c <= 24)
          return text_(std::string(1, (char) ('A' + c - 1)));
        return text_(std::to_string(c));
      }
      case 215:
      case 216: {
        char s[24];
        snprintf(s, sizeof(s), "{0:%d}{1:%d}", data[0] >> 4, data[0] & 15);
        return text_(s);
      }
      case 201:
      case 217: {
        static const char *const t[] = {"Fan Only",         "Heating",          "Cooling",          "Auto",
                                        "Ventilation",      "Auto Cool",        "Auto Heat",        "Dry",
                                        "Aux.",             "Cooling Storage",  "Heating Storage",  "UseStrdThrm(cl)1",
                                        "UseStrdThrm(cl)2", "UseStrdThrm(cl)3", "UseStrdThrm(cl)4", "UseStrdThrm(ht)1",
                                        "UseStrdThrm(ht)2", "UseStrdThrm(ht)3", "UseStrdThrm(ht)4"};
        return text_(data[0] < 19 ? t[data[0]] : "???");
      }
      case 218: {
        static const char *const t[] = {
            "Stop control",           "Restart stand by",           "Before startup control",
            "Cool startup control",   "Cool normal control",        "Cool before oil return",
            "Cool oil return",        "Cool after oil return",      "Cool pump down residual",
            "HR startup control",     "HR normal control",          "HR before oil return",
            "HR oil return",          "HR after oil return",        "HR before defrost",
            "HR defrost",             "HR after defrost",           "HR pump down residual",
            "Heat startup control",   "Heat fixed control",         "Heat before oil return",
            "Heat oil return",        "Heat after oil return",      "Heat before defrost",
            "Heat defrost",           "Heat after defrost",         "Heat pump down residual",
            "Mode switch (cool to heat)", "Mode switch (heat to cool)", "Rotation control",
            "4 way valve initial control"};
        unsigned c = get_unsigned(data, size, 1);
        return text_(c < sizeof(t) / sizeof(t[0]) ? t[c] : "???");
      }
      case 219: {
        static const uint8_t code[] = {17, 22, 28, 36, 45, 56, 71, 80, 90, 112, 125, 140, 160, 180, 224,
                                       251, 252, 253, 254, 255};
        static const char *const kw[] = {"15",  "20",  "25",  "32",  "40",  "50",  "63",  "71",  "80",  "100",
                                         "112", "125", "140", "160", "200", "250", "290", "300", "310", "320"};
        int c = data[0];
        if (c == 0 || c == 50 || c == 63)
          return text_("---");
        for (size_t i = 0; i < sizeof(code); i++)
          if (code[i] == c)
            return text_(kw[i]);
        return text_(std::to_string(c));
      }
      case 300:
      case 301:
      case 302:
      case 303:
      case 304:
      case 305:
      case 306:
      case 307:
        return bool_((data[0] & (1 << (conv % 10))) != 0);
      case 310: d = (data[0] & 0x70) >> 4; break;
      case 311: d = data[0] & 0x07; break;
      case 312:
        d = ((7 & data[0] >> 4) + (15 & data[0])) / 16.0;
        if (data[0] & 128)
          d *= -1.0;
        break;
      case 313: {
        static const char *const t[] = {"Fan Only", "Heating",   "Cooling",   "Auto",
                                        "Ventilation", "Auto Heat", "Auto Cool", "Dry"};
        unsigned c = data[0] >> 4;
        return text_(c < 8 ? t[c] : "Aux.");
      }
      case 314: {
        static const char conv51[32] = {'-', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A',
                                        'C', 'E', 'F', '*', 'H', 'J', 'E', 'L', 'U', '*', 'P',
                                        'P', 'P', 'U', '2', 'A', 'E', 'F', '*', '*', '*'};
        if (size < 2)
          return text_("---");
        unsigned val = ((unsigned) data[1] << 8) | data[0];
        int i4 = val & 0x3F;
        char s[4] = {conv51[(val >> 11) & 0x1F], conv51[(val >> 6) & 0x1F],
                     i4 > 32 ? conv51[0] : (i4 < 32 ? conv51[i4] : '?'), 0};
        return text_(s);
      }
      case 315: {
        static const char *const t[] = {"Stop", "Heating", "Cooling", "??", "DHW", "Heating + DHW", "Cooling + DHW"};
        unsigned c = (data[0] & 0xF0) >> 4;
        return text_(c < 7 ? t[c] : "-");
      }
      case 316: {
        static const char *const t[] = {"H/P only", "Hybrid", "Boiler only"};
        unsigned c = (data[0] & 0xF0) >> 4;
        return text_(c < 3 ? t[c] : "Unknown");
      }
      case 317:
      case 323:
      case 336:
        d = data[0];
        break;
      case 401: d = press2temp(get_signed(data, size, 0)); break;
      case 402: d = press2temp(get_signed(data, size, 1)); break;
      case 403: d = press2temp(get_signed(data, size, 0) / 256.0); break;
      case 404: d = press2temp(get_signed(data, size, 1) / 256.0); break;
      case 405: d = press2temp(get_signed(data, size, 0) * 0.1); break;
      case 406: d = press2temp(get_signed(data, size, 1) * 0.1); break;
      case 800:
      case 801:
      case 802:
      case 803:
      case 804: {
        static const char *const t[] = {"---", "R410A", "R32", "R22", "R134a"};
        this->refrigerant = (Refrigerant) conv;
        return text_(t[conv - 800]);
      }
      default:
        return v;  // INVALID: conversion not available
    }
    v.kind = ValueKind::NUMBER;
    v.number = (float) d;
    return v;
  }

  static uint16_t get_unsigned(const uint8_t *data, int size, int flag) {
    if (size == 1)
      return data[0];
    if (flag == 0)
      return (uint16_t) ((data[1] << 8) | data[0]);
    return (uint16_t) ((data[0] << 8) | data[1]);
  }

  static int16_t get_signed(const uint8_t *data, int size, int flag) {
    uint16_t n = get_unsigned(data, size, flag);
    // A single byte stays unsigned, as in ESPAltherma (only bit 15 marks a negative value)
    return (int16_t) n;
  }

 protected:
  static Value text_(const std::string &s) {
    Value v;
    v.kind = ValueKind::TEXT;
    v.text = s;
    return v;
  }
  static Value bool_(bool b) {
    Value v;
    v.kind = ValueKind::BOOL;
    v.state = b;
    return v;
  }

  double press2temp(double p) const {
    switch (this->refrigerant) {
      case Refrigerant::R410A:
        return -8.4448460086362E-07 * pow(p, 6) + 0.000112833751855216 * pow(p, 5) - 0.00584955138495273 * pow(p, 4) +
               0.149130933664499 * pow(p, 3) - 1.99373679674187 * p * p + 16.0504901396224 * p - 53.2854030662239;
      case Refrigerant::R22:
        return -5.0232146925414E-06 * pow(p, 6) + 0.000475561916201275 * pow(p, 5) - 0.0175087997648461 * pow(p, 4) +
               0.318620413943417 * pow(p, 3) - 3.0844373741869 * p * p + 19.0217687284902 * p - 42.3282361912494;
      case Refrigerant::R134A: {
        double kpa = p * 98.0665 + 101.325;
        if (kpa <= 0)
          return -99.0;
        return 2184.4 / (14.570 - log(kpa)) - 245.7;
      }
      default:  // R32
        return -2.6989493795556E-07 * pow(p, 6) + 4.26383417104661E-05 * pow(p, 5) -
               0.00262978346547749 * pow(p, 4) + 0.0805858127503585 * pow(p, 3) - 1.31924457284073 * p * p +
               13.4157368435437 * p - 51.1813342993155;
    }
  }
};

}  // namespace daikin_x10a
}  // namespace esphome
