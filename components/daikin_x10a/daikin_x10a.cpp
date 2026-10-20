#include "daikin_x10a.h"

#include <algorithm>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daikin_x10a {

static const char *const TAG = "daikin_x10a";
static const uint32_t REPLY_TIMEOUT_MS = 300;  // leave the unit 300 ms to answer, as ESPAltherma does

static uint8_t crc(const uint8_t *data, size_t len) {
  uint8_t b = 0;
  for (size_t i = 0; i < len; i++)
    b += data[i];
  return ~b;
}

void DaikinX10A::setup() {
  for (auto &l : this->labels_)
    if (std::find(this->registries_.begin(), this->registries_.end(), l.registry) == this->registries_.end())
      this->registries_.push_back(l.registry);
}

void DaikinX10A::dump_config() {
  ESP_LOGCONFIG(TAG, "Daikin X10A:");
  ESP_LOGCONFIG(TAG, "  Protocol: %c", this->protocol_);
  ESP_LOGCONFIG(TAG, "  Values: %u in %u registers", (unsigned) this->labels_.size(),
                (unsigned) this->registries_.size());
  LOG_UPDATE_INTERVAL(this);
}

Label &DaikinX10A::add_label_(uint8_t reg, uint8_t offset, uint16_t conv, uint8_t size) {
  Label l{};
  l.registry = reg;
  l.offset = offset;
  l.conv = conv;
  l.size = size;
  this->labels_.push_back(l);
  return this->labels_.back();
}

#ifdef USE_SENSOR
void DaikinX10A::add_sensor(sensor::Sensor *s, uint8_t reg, uint8_t offset, uint16_t conv, uint8_t size) {
  this->add_label_(reg, offset, conv, size).sensor = s;
}
#endif
#ifdef USE_BINARY_SENSOR
void DaikinX10A::add_binary_sensor(binary_sensor::BinarySensor *s, uint8_t reg, uint8_t offset, uint16_t conv,
                                   uint8_t size) {
  this->add_label_(reg, offset, conv, size).binary_sensor = s;
}
#endif
#ifdef USE_TEXT_SENSOR
void DaikinX10A::add_text_sensor(text_sensor::TextSensor *s, uint8_t reg, uint8_t offset, uint16_t conv,
                                 uint8_t size) {
  this->add_label_(reg, offset, conv, size).text_sensor = s;
}
#endif

bool DaikinX10A::is_connected() const {
  return this->ever_succeeded_ && millis() - this->last_success_ < 3 * this->get_update_interval() + 5000;
}

void DaikinX10A::update() {
#ifdef USE_BINARY_SENSOR
  if (this->connected_sensor_ != nullptr)
    this->connected_sensor_->publish_state(this->is_connected());
#endif
  if (this->registries_.empty())
    return;
  if (this->state_ != State::IDLE) {
    ESP_LOGW(TAG, "Previous poll cycle still running, skipping");
    return;
  }
  this->current_ = 0;
  this->cycle_ok_ = true;
  this->state_ = State::SEND;
}

size_t DaikinX10A::reply_length_(uint8_t reg) const {
  if (this->protocol_ == 'I')
    return 12;  // replaced by the real length from the third byte
  switch (reg) {
    case 0x50:
      return 6;
    case 0x56:
      return 4;
    default:
      return 18;
  }
}

void DaikinX10A::send_query_() {
  uint8_t reg = this->registries_[this->current_];
  uint8_t frame[4];
  size_t len;
  if (this->protocol_ == 'S') {
    frame[0] = 0x02;
    frame[1] = reg;
    frame[2] = crc(frame, 2);
    len = 3;
  } else {
    frame[0] = 0x03;
    frame[1] = 0x40;
    frame[2] = reg;
    frame[3] = crc(frame, 3);
    len = 4;
  }
  uint8_t discard;
  while (this->available())
    this->read_byte(&discard);  // left over from an earlier exchange
  this->write_array(frame, len);
  this->received_ = 0;
  this->expected_ = this->reply_length_(reg);
  this->sent_at_ = millis();
  this->state_ = State::WAIT;
}

void DaikinX10A::loop() {
  if (this->state_ == State::SEND) {
    this->send_query_();
    return;
  }
  if (this->state_ != State::WAIT)
    return;

  while (this->available() && this->received_ < this->expected_) {
    if (this->received_ >= sizeof(this->buffer_)) {
      this->fail_reply_("reply too long");
      return;
    }
    this->read_byte(&this->buffer_[this->received_++]);
    if (this->protocol_ == 'I' && this->received_ == 3)
      this->expected_ = this->buffer_[2] + 2;  // real length, see ESPAltherma doc/Daikin I protocol.md
    if (this->received_ == 2 && this->buffer_[0] == 0x15 && this->buffer_[1] == 0xEA) {
      this->fail_reply_("unit did not understand the request");
      return;
    }
  }
  if (this->received_ >= this->expected_) {
    this->finish_reply_();
  } else if (millis() - this->sent_at_ > REPLY_TIMEOUT_MS) {
    this->fail_reply_(this->received_ == 0 ? "no reply, check the wiring" : "reply incomplete");
  }
}

void DaikinX10A::finish_reply_() {
  uint8_t reg = this->registries_[this->current_];
  size_t len = this->received_;
  if (this->log_raw_)
    ESP_LOGD(TAG, "Register 0x%02X: %s", reg, format_hex_pretty(this->buffer_, len).c_str());
  if (crc(this->buffer_, len - 1) != this->buffer_[len - 1]) {
    this->fail_reply_("wrong CRC");
    return;
  }
  // Values start after the header: 3 bytes in protocol I, 1 in protocol S
  if (this->protocol_ == 'S')
    this->publish_(this->buffer_ + 1, len - 2, this->buffer_[0]);
  else
    this->publish_(this->buffer_ + 3, len - 4, this->buffer_[1]);
  this->next_register_();
}

void DaikinX10A::fail_reply_(const char *reason) {
  ESP_LOGW(TAG, "Register 0x%02X: %s (%u bytes)", this->registries_[this->current_], reason,
           (unsigned) this->received_);
  this->cycle_ok_ = false;
  this->next_register_();
}

void DaikinX10A::next_register_() {
  this->current_++;
  if (this->current_ < this->registries_.size()) {
    this->state_ = State::SEND;
    return;
  }
  this->state_ = State::IDLE;
  if (this->cycle_ok_) {
    this->last_success_ = millis();
    this->ever_succeeded_ = true;
  }
}

void DaikinX10A::publish_(const uint8_t *payload, uint8_t len, uint8_t reg) {
  for (auto &l : this->labels_) {
    if (l.registry != reg)
      continue;
    if (l.offset + l.size > len) {
      ESP_LOGW(TAG, "Register 0x%02X: offset %u beyond reply (%u bytes)", reg, l.offset, len);
      continue;
    }
    Value v = this->converter_.convert(l.conv, payload + l.offset, l.size);
    if (v.kind == ValueKind::INVALID) {
      ESP_LOGW(TAG, "Register 0x%02X offset %u: conversion %u not available", reg, l.offset, l.conv);
      continue;
    }
#ifdef USE_SENSOR
    if (l.sensor != nullptr)
      l.sensor->publish_state(v.kind == ValueKind::NUMBER ? v.number : (v.kind == ValueKind::BOOL ? v.state : NAN));
#endif
#ifdef USE_BINARY_SENSOR
    if (l.binary_sensor != nullptr && v.kind == ValueKind::BOOL)
      l.binary_sensor->publish_state(v.state);
#endif
#ifdef USE_TEXT_SENSOR
    if (l.text_sensor != nullptr) {
      if (v.kind == ValueKind::TEXT) {
        l.text_sensor->publish_state(v.text);
      } else if (v.kind == ValueKind::BOOL) {
        l.text_sensor->publish_state(v.state ? "ON" : "OFF");
      } else {
        char s[16];
        snprintf(s, sizeof(s), "%g", v.number);
        l.text_sensor->publish_state(s);
      }
    }
#endif
  }
}

}  // namespace daikin_x10a
}  // namespace esphome
