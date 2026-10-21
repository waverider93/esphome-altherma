#include "daikin_x10a.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

// ESPAltherma's catalog, survey, detection and layout check (MIT License, Copyright (c) 2020 Raomin).
// They define functions in their headers: this file is the only one that includes them.
#define DAIKIN_X10A_ESPALTHERMA_IMPL
#include "catalog.h"
#include "survey.h"
#include "detect.h"
#include "layoutfix.h"

#ifdef USE_DAIKIN_X10A_DIAGRAM
#include "diagram_html.h"
#endif

namespace esphome {
namespace daikin_x10a {

static const char *const TAG = "daikin_x10a";
static const uint32_t REPLY_TIMEOUT_MS = 300;    // leave the unit 300 ms to answer, as ESPAltherma does
static const uint32_t SURVEY_DELAY_MS = 5000;    // after boot, so that the unit and the UART are settled
static const uint32_t SURVEY_RETRY_MS = 60000;   // when no heat pump answered
static const size_t DIAGRAM_MAX_VALUES = 128;

struct DiagramValue {
  int16_t entry;
  char value[24];
};

struct Engine {
  Survey survey;
  DetectResult detection;
  bool detected{false};
  bool restricted{false};  // detected with less than high confidence: only values the close models read alike
  LayoutChecker layout;
  LayoutCycle cycle;
  std::vector<CatalogFix> fixes;
  std::vector<DiagramValue> diagram;
};

// Stored layout corrections of a model
struct FixStore {
  int8_t model;
  uint8_t count;
  uint8_t reg[LAYOUT_MAX_REGS];
  int8_t fix_model[LAYOUT_MAX_REGS];
} __attribute__((packed));

static uint8_t crc(const uint8_t *data, size_t len) {
  uint8_t b = 0;
  for (size_t i = 0; i < len; i++)
    b += data[i];
  return ~b;
}

static DaikinX10A *survey_hub = nullptr;  // NOLINT
static bool survey_query(uint8_t reg, unsigned char *buffer, char protocol) {
  return survey_hub->query_blocking(reg, buffer, protocol);
}

static void format_value(const Value &v, char *out, size_t size) {
  switch (v.kind) {
    case ValueKind::TEXT:
      snprintf(out, size, "%s", v.text.c_str());
      break;
    case ValueKind::BOOL:
      snprintf(out, size, "%s", v.state ? "ON" : "OFF");
      break;
    case ValueKind::NUMBER:
      snprintf(out, size, "%g", v.number);
      break;
    default:
      out[0] = 0;
  }
}

#ifdef USE_DAIKIN_X10A_DIAGRAM
static void json_string(std::string &out, const char *s) {
  out += '"';
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') {
      out += '\\';
      out += *s;
    } else if ((uint8_t) *s < 0x20) {
      out += ' ';
    } else {
      out += *s;
    }
  }
  out += '"';
}
#endif

// --- Setup ---

void DaikinX10A::setup() {
  this->engine_ = new Engine();  // NOLINT(cppcoreguidelines-owning-memory)
  this->language_ = catalogLanguage(this->language_name_);
  this->fix_pref_ = global_preferences->make_preference<FixStore>(fnv1_hash("daikin_x10a_fixes"));
  int configured = catalogFindModel(this->model_name_);
  if (configured >= 0) {
    this->model_ = configured;
    this->load_fixes_();
  }
  this->apply_model_();
  if (this->detect_) {
    this->survey_pending_ = true;
    this->survey_at_ = millis() + SURVEY_DELAY_MS;
  }
#ifdef USE_DAIKIN_X10A_DIAGRAM
  if (this->base_ != nullptr) {
    class Handler : public AsyncWebHandler {
     public:
      explicit Handler(DaikinX10A *hub) : hub_(hub) {}
      bool canHandle(AsyncWebServerRequest *request) const override {
        if (request->method() != HTTP_GET)
          return false;
        char buf[AsyncWebServerRequest::URL_BUF_SIZE];
        auto url = request->url_to(buf);
        return url == "/altherma" || url == "/altherma/" || url == "/altherma/values.json";
      }
      void handleRequest(AsyncWebServerRequest *request) override {
        char buf[AsyncWebServerRequest::URL_BUF_SIZE];
        auto url = request->url_to(buf);
        if (url == "/altherma") {
          request->redirect("/altherma/");  // the page loads values.json relative to itself
          return;
        }
        if (url == "/altherma/values.json") {
          auto *response = request->beginResponse(200, "application/json", this->hub_->diagram_json());
          response->addHeader("Cache-Control", "no-store");
          request->send(response);
          return;
        }
        auto *response = request->beginResponse(200, "text/html", DIAGRAM_HTML_GZ, sizeof(DIAGRAM_HTML_GZ));
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      }

     protected:
      DaikinX10A *hub_;
    };
    this->base_->add_handler(new Handler(this));  // NOLINT(cppcoreguidelines-owning-memory)
    // Start the server ourselves: without web_server (the ESPHome dashboard) nobody else does. Counted, so a
    // web_server that starts it too does no harm. Only after setup: the network stack must exist (see loop()).
    this->web_start_pending_ = true;
  }
#endif
}

void DaikinX10A::dump_config() {
  ESP_LOGCONFIG(TAG, "Daikin X10A:");
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_ >= 0 ? CATALOG_MODELS[this->model_].name : "auto");
  ESP_LOGCONFIG(TAG, "  Detection: %s, layout check: %s, language: %s", YESNO(this->detect_),
                YESNO(this->layout_check_), this->language_name_);
  ESP_LOGCONFIG(TAG, "  Protocol: %c", this->protocol_);
  ESP_LOGCONFIG(TAG, "  Values: %u in %u registers", (unsigned) this->labels_.size(),
                (unsigned) this->registries_.size());
  LOG_UPDATE_INTERVAL(this);
}

Label &DaikinX10A::add_label_(const char *label, int reg_filter, uint8_t reg, uint8_t offset, uint16_t conv,
                              uint8_t size) {
  Label l{};
  l.label = label;
  l.reg_filter = reg_filter;
  l.registry = reg;
  l.offset = offset;
  l.conv = conv;
  l.size = size;
  l.active = label == nullptr;
  this->labels_.push_back(l);
  return this->labels_.back();
}

#ifdef USE_SENSOR
void DaikinX10A::add_sensor(sensor::Sensor *s, const char *label, int reg_filter, uint8_t reg, uint8_t offset,
                            uint16_t conv, uint8_t size) {
  this->add_label_(label, reg_filter, reg, offset, conv, size).sensor = s;
}
#endif
#ifdef USE_BINARY_SENSOR
void DaikinX10A::add_binary_sensor(binary_sensor::BinarySensor *s, const char *label, int reg_filter, uint8_t reg,
                                   uint8_t offset, uint16_t conv, uint8_t size) {
  this->add_label_(label, reg_filter, reg, offset, conv, size).binary_sensor = s;
}
#endif
#ifdef USE_TEXT_SENSOR
void DaikinX10A::add_text_sensor(text_sensor::TextSensor *s, const char *label, int reg_filter, uint8_t reg,
                                 uint8_t offset, uint16_t conv, uint8_t size) {
  this->add_label_(label, reg_filter, reg, offset, conv, size).text_sensor = s;
}
#endif

bool DaikinX10A::is_connected() const {
  return this->ever_succeeded_ && millis() - this->last_success_ < 3 * this->get_update_interval() + 5000;
}

const char *DaikinX10A::model_name() const { return this->model_ >= 0 ? CATALOG_MODELS[this->model_].name : ""; }

// --- Model: which values are read where ---

bool DaikinX10A::safe_entry_(int entry) const {
  const Engine &en = *this->engine_;
  if (!en.restricted)
    return true;
  // Like detectSafeKeys(): every model scoring close to the detected one must read this value the same way
  uint32_t key = catalogKey(CATALOG_ENTRIES[entry]);
  int best = en.detection.allScores[this->model_];
  for (int m = 0; m < CATALOG_MODEL_COUNT; m++) {
    if (m == this->model_ || en.detection.allScores[m] <= best - DETECT_MARGIN_HIGH)
      continue;
    if (!detectModelHasKey(m, key))
      return false;
  }
  return true;
}

void DaikinX10A::apply_model_() {
  Engine &en = *this->engine_;
  int model = this->model_;
  const CatalogFix *fixes = en.fixes.data();
  size_t fix_count = en.fixes.size();

  if (model >= 0) {
    this->protocol_ = CATALOG_MODELS[model].protocol;
    uint16_t refrigerant = this->refrigerant_;
    for (int i = 0; refrigerant == 0 && i < CATALOG_ENTRY_COUNT; i++) {
      const CatalogEntry &e = CATALOG_ENTRIES[i];
      if ((e.flags & CATALOG_FLAG_ALWAYS) && catalogInModel(e, model))
        refrigerant = e.conv;
    }
    this->converter_.refrigerant = (Refrigerant) (refrigerant ? refrigerant : 802);
  } else if (this->refrigerant_ != 0) {
    this->converter_.refrigerant = (Refrigerant) this->refrigerant_;
  }

  for (auto &l : this->labels_) {
    if (l.label == nullptr)
      continue;
    l.active = false;
    l.entry = -1;
    if (model < 0)
      continue;
    bool unsafe = false;
    for (int i = 0; i < CATALOG_ENTRY_COUNT; i++) {
      const CatalogEntry &e = CATALOG_ENTRIES[i];
      if ((e.flags & CATALOG_FLAG_ALWAYS) || strcmp(catalogLabel(e), l.label) != 0)
        continue;
      if (l.reg_filter >= 0 && e.reg != l.reg_filter)
        continue;
      if (!catalogInModel(e, catalogModelFor(e.reg, model, fixes, fix_count)))
        continue;
      if (!this->safe_entry_(i)) {
        unsafe = true;
        break;
      }
      l.entry = i;
      l.registry = e.reg;
      l.offset = e.offset;
      l.conv = e.conv;
      l.size = e.size;
      l.active = true;
      break;
    }
    if (unsafe) {
      ESP_LOGW(TAG, "'%s' not read: the detected model is uncertain and close models read it elsewhere. Set model:",
               l.label);
    } else if (!l.active) {
      ESP_LOGW(TAG, "'%s' is not a value of %s", l.label, CATALOG_MODELS[model].name);
    }
  }

  // The diagram page shows the recommended values of the model and the configured ones
  en.diagram.clear();
#ifdef USE_DAIKIN_X10A_DIAGRAM
  if (model >= 0) {
    for (int i = 0; i < CATALOG_ENTRY_COUNT && en.diagram.size() < DIAGRAM_MAX_VALUES; i++) {
      const CatalogEntry &e = CATALOG_ENTRIES[i];
      if ((e.flags & CATALOG_FLAG_ALWAYS) || !catalogInModel(e, catalogModelFor(e.reg, model, fixes, fix_count)))
        continue;
      bool configured = std::any_of(this->labels_.begin(), this->labels_.end(),
                                    [i](const Label &l) { return l.active && l.entry == i; });
      if ((!catalogRecommended(e) && !configured) || !this->safe_entry_(i))
        continue;
      DiagramValue v{};
      v.entry = (int16_t) i;
      en.diagram.push_back(v);
    }
  }
#endif

  this->registries_.clear();
  auto add_reg = [this](uint8_t reg) {
    if (std::find(this->registries_.begin(), this->registries_.end(), reg) == this->registries_.end())
      this->registries_.push_back(reg);
  };
  for (auto &l : this->labels_)
    if (l.active)
      add_reg(l.registry);
  for (auto &v : en.diagram)
    add_reg(CATALOG_ENTRIES[v.entry].reg);
  std::sort(this->registries_.begin(), this->registries_.end());
  this->build_diagram_json_();
}

// --- Survey and detection ---

bool DaikinX10A::query_blocking(uint8_t reg, uint8_t *buffer, char protocol) {
  uint8_t frame[4];
  size_t len = this->build_query_(reg, protocol, frame);
  uint8_t discard;
  while (this->available())
    this->read_byte(&discard);
  this->write_array(frame, len);
  size_t expected = this->reply_length_(reg, protocol), received = 0;
  uint32_t start = millis();
  while (received < expected && millis() - start < REPLY_TIMEOUT_MS) {
    App.feed_wdt();
    if (!this->available()) {
      delay(1);
      continue;
    }
    this->read_byte(&buffer[received++]);
    if (protocol == 'I' && received == 3)
      expected = std::min<size_t>(buffer[2] + 2, SURVEY_BUFFER_SIZE);
    if (received == 2 && buffer[0] == 0x15 && buffer[1] == 0xEA)
      return false;
  }
  if (this->log_raw_ && received > 0)
    ESP_LOGD(TAG, "Survey 0x%02X: %s", reg, format_hex_pretty(buffer, received).c_str());
  return received == expected && crc(buffer, received - 1) == buffer[received - 1];
}

// Two zones: the mixed water temperature reads something (single-zone units answer 0), as ESPAltherma does
static bool survey_two_zones(const Survey &s, int model, Converter &converter) {
  for (int i = 0; model >= 0 && i < CATALOG_ENTRY_COUNT; i++) {
    const CatalogEntry &e = CATALOG_ENTRIES[i];
    if (!catalogInModel(e, model) || strncmp(catalogLabel(e), "Mixed water temp.", 17) != 0)
      continue;
    const SurveyReg *r = s.find(e.reg);
    if (r == nullptr || !r->answered || e.offset + e.size > r->len)
      continue;
    Value v = converter.convert(e.conv, r->payload + e.offset, e.size);
    if (v.kind == ValueKind::NUMBER && v.number != 0)
      return true;
  }
  return false;
}

void DaikinX10A::run_survey_() {
  Engine &en = *this->engine_;
  this->survey_pending_ = false;
  ESP_LOGI(TAG, "Survey: reading every known register once");
  survey_hub = this;
  surveyRun(en.survey, survey_query);
  detectModel(en.survey, en.detection);
  en.detected = true;
  if (en.survey.protocol == 0) {
    ESP_LOGW(TAG, "Survey: no heat pump answered, check the wiring to X10A. Trying again in a minute.");
    this->survey_pending_ = true;
    this->survey_at_ = millis() + SURVEY_RETRY_MS;
    this->publish_info_();
    return;
  }
  const DetectResult &d = en.detection;
  ESP_LOGI(TAG, "Survey: protocol %c, %u registers. Detected %s (%s confidence, %s)", en.survey.protocol,
           en.survey.count, d.model >= 0 ? CATALOG_MODELS[d.model].name : "nothing",
           detectConfidenceName(d.confidence), detectMethod(d));

  int configured = catalogFindModel(this->model_name_);
  if (configured >= 0) {
    if (d.model >= 0 && d.confidence >= DETECT_MEDIUM && detectDecodeHash(d.model) != detectDecodeHash(configured))
      ESP_LOGW(TAG, "The heat pump looks like %s, not the configured %s", CATALOG_MODELS[d.model].name,
               CATALOG_MODELS[configured].name);
  } else {
    if (d.model < 0) {
      ESP_LOGW(TAG, "Model not detected: set model: in the YAML");
      this->publish_info_();
      return;
    }
    en.restricted = d.confidence != DETECT_HIGH;
    if (en.restricted)
      ESP_LOGW(TAG, "Model not certain: only the values the close models read alike are published. "
                    "Confirm the model with model: in the YAML.");
    if (this->model_ != d.model) {
      this->model_ = d.model;
      this->load_fixes_();
    }
  }
  catalogTwoZones() = survey_two_zones(en.survey, this->model_, this->converter_);
  if (this->layout_check_)
    layoutCheckInit(en.layout, this->model_);
  this->apply_model_();
  this->publish_info_();
  // The survey replies count as a first poll cycle for the layout check
  if (en.survey.protocol == 'I') {
    en.cycle.clear();
    for (uint8_t i = 0; i < en.survey.count; i++) {
      const SurveyReg &r = en.survey.regs[i];
      if (r.answered)
        en.cycle.add(r.id, r.payload, r.len);
    }
    this->check_layout_();
  }
}

// --- Layout check ---

void DaikinX10A::load_fixes_() {
  Engine &en = *this->engine_;
  en.fixes.clear();
  FixStore store{};
  if (!this->fix_pref_.load(&store) || store.model != this->model_)
    return;
  for (uint8_t i = 0; i < store.count && i < LAYOUT_MAX_REGS; i++) {
    if (store.fix_model[i] >= 0 && store.fix_model[i] < CATALOG_MODEL_COUNT)
      en.fixes.push_back({store.reg[i], store.fix_model[i]});
  }
  for (auto &f : en.fixes)
    ESP_LOGI(TAG, "Register 0x%02X read with the layout of %s (stored)", f.reg, CATALOG_MODELS[f.model].name);
}

void DaikinX10A::save_fixes_() {
  Engine &en = *this->engine_;
  FixStore store{};
  store.model = (int8_t) this->model_;
  for (auto &f : en.fixes) {
    if (store.count >= LAYOUT_MAX_REGS)
      break;
    store.reg[store.count] = f.reg;
    store.fix_model[store.count] = (int8_t) f.model;
    store.count++;
  }
  this->fix_pref_.save(&store);
}

void DaikinX10A::check_layout_() {
  Engine &en = *this->engine_;
  if (!this->layout_check_ || this->model_ < 0 || en.layout.model != this->model_ || en.layout.count == 0 ||
      en.cycle.count == 0)
    return;
  layoutCheckCycle(en.layout, en.cycle);
  bool changed = false;
  for (uint8_t i = 0; i < en.layout.count; i++) {
    LayoutCheck &rc = en.layout.regs[i];
    bool fixed = std::any_of(en.fixes.begin(), en.fixes.end(), [&rc](const CatalogFix &f) { return f.reg == rc.reg; });
    int k = fixed ? -1 : layoutCheckDecide(rc);
    if (k < 0 || en.fixes.size() >= LAYOUT_MAX_REGS)
      continue;
    const LayoutCandidate &a = rc.c[0], &c = rc.c[k];
    ESP_LOGW(TAG, "Register 0x%02X does not match %s on this unit (%u/%u plausible values): read with the layout of %s",
             rc.reg, CATALOG_MODELS[this->model_].name, a.plausible, a.checks, CATALOG_MODELS[c.model].name);
    en.fixes.push_back({rc.reg, c.model});
    changed = true;
  }
  if (changed) {
    this->save_fixes_();
    this->apply_model_();
    this->publish_info_();
  }
}

void DaikinX10A::publish_info_() {
#ifdef USE_TEXT_SENSOR
  Engine &en = *this->engine_;
  if (this->model_sensor_ != nullptr)
    this->model_sensor_->publish_state(this->model_ >= 0 ? CATALOG_MODELS[this->model_].name : "unknown");
  if (this->detection_sensor_ != nullptr && en.detected) {
    const DetectResult &d = en.detection;
    char s[160];
    if (en.survey.protocol == 0) {
      snprintf(s, sizeof(s), "no answer");
    } else {
      snprintf(s, sizeof(s), "%s (%s, %s)", d.model >= 0 ? CATALOG_MODELS[d.model].name : "unknown",
               detectConfidenceName(d.confidence), detectMethod(d));
    }
    this->detection_sensor_->publish_state(s);
  }
  if (this->layout_sensor_ != nullptr) {
    std::string s;
    for (auto &f : en.fixes) {
      char part[96];
      snprintf(part, sizeof(part), "%s0x%02X: %s", s.empty() ? "" : "; ", f.reg, CATALOG_MODELS[f.model].name);
      s += part;
    }
    this->layout_sensor_->publish_state(s.empty() ? "as defined" : s);
  }
#endif
}

// --- Polling ---

void DaikinX10A::update() {
  if (this->registries_.empty() || this->survey_pending_)
    return;
  if (this->state_ != State::IDLE) {
    ESP_LOGW(TAG, "Previous poll cycle still running, skipping");
    return;
  }
  this->engine_->cycle.clear();
  this->current_ = 0;
  this->cycle_ok_ = true;
  this->state_ = State::SEND;
}

size_t DaikinX10A::reply_length_(uint8_t reg, char protocol) const {
  if (protocol == 'I')
    return 12;  // replaced by the real length from the third byte
  return surveyReplyLenS(reg);
}

size_t DaikinX10A::build_query_(uint8_t reg, char protocol, uint8_t *frame) const {
  if (protocol == 'S') {
    frame[0] = 0x02;
    frame[1] = reg;
    frame[2] = crc(frame, 2);
    return 3;
  }
  frame[0] = 0x03;
  frame[1] = 0x40;
  frame[2] = reg;
  frame[3] = crc(frame, 3);
  return 4;
}

void DaikinX10A::send_query_() {
  uint8_t reg = this->registries_[this->current_];
  uint8_t frame[4];
  size_t len = this->build_query_(reg, this->protocol_, frame);
  uint8_t discard;
  while (this->available())
    this->read_byte(&discard);  // left over from an earlier exchange
  this->write_array(frame, len);
  this->received_ = 0;
  this->expected_ = this->reply_length_(reg, this->protocol_);
  this->sent_at_ = millis();
  this->state_ = State::WAIT;
}

void DaikinX10A::loop() {
#ifdef USE_DAIKIN_X10A_DIAGRAM
  if (this->web_start_pending_ && App.is_setup_complete()) {
    this->web_start_pending_ = false;
    this->base_->init();
  }
#endif
  if (this->survey_pending_ && this->state_ == State::IDLE && (int32_t) (millis() - this->survey_at_) >= 0) {
    this->run_survey_();
    return;
  }
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
  if (this->protocol_ == 'S') {
    this->publish_(this->buffer_ + 1, len - 2, this->buffer_[0]);
  } else {
    this->publish_(this->buffer_ + 3, len - 4, this->buffer_[1]);
    this->engine_->cycle.add(this->buffer_[1], this->buffer_ + 3, len - 4);
  }
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
  this->check_layout_();
  this->build_diagram_json_();
}

void DaikinX10A::publish_(const uint8_t *payload, uint8_t len, uint8_t reg) {
  for (auto &l : this->labels_) {
    if (!l.active || l.registry != reg)
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
      char s[32];
      format_value(v, s, sizeof(s));
      l.text_sensor->publish_state(s);
    }
#endif
  }
  for (auto &d : this->engine_->diagram) {
    const CatalogEntry &e = CATALOG_ENTRIES[d.entry];
    if (e.reg != reg || e.offset + e.size > len)
      continue;
    format_value(this->converter_.convert(e.conv, payload + e.offset, e.size), d.value, sizeof(d.value));
  }
}

// --- Diagram page ---

void DaikinX10A::build_diagram_json_() {
#ifdef USE_DAIKIN_X10A_DIAGRAM
  std::string s = "{\"model\":";
  json_string(s, this->model_name());
  s += ",\"lang\":";
  json_string(s, this->language_name_);
  s += this->is_connected() ? ",\"connected\":true" : ",\"connected\":false";
  s += ",\"values\":[";
  bool first = true;
  for (auto &d : this->engine_->diagram) {
    const CatalogEntry &e = CATALOG_ENTRIES[d.entry];
    s += first ? "{\"l\":" : ",{\"l\":";
    first = false;
    json_string(s, catalogLabel(e));
    s += ",\"n\":";
    json_string(s, catalogName(e, this->language_));
    s += ",\"v\":";
    json_string(s, d.value);
    s += '}';
  }
  s += ']';
  if (this->control_json_) {
    s += ",\"control\":";
    s += this->control_json_();
  }
  s += '}';
  LockGuard guard(this->json_lock_);
  this->json_.swap(s);
#endif
}

std::string DaikinX10A::diagram_json() {
  LockGuard guard(this->json_lock_);
  return this->json_;
}

}  // namespace daikin_x10a
}  // namespace esphome
