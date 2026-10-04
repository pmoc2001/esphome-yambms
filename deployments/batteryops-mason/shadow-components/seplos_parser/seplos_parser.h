#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#include <vector>
#include <deque>
#include <algorithm>
#include "pic_status.h"
#include "supervised_trial.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
//#ifdef USE_BINARY_SENSOR
//#include "esphome/components/binary_sensor/binary_sensor.h"
//#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace seplos_parser {

class SeplosParser : public uart::UARTDevice, public Component {
#ifdef USE_SENSOR
 protected:
  std::vector<sensor::Sensor *> sensors_;

 public:
  void register_sensor(sensor::Sensor *obj) { this->sensors_.push_back(obj); }
#endif
//#ifdef USE_BINARY_SENSOR
// protected:
//  std::vector<binary_sensor::BinarySensor *> binary_sensors_;
//
// public:
//  void register_binary_sensor(binary_sensor::BinarySensor *obj) { this->binary_sensors_.push_back(obj); }
//#endif
#ifdef USE_TEXT_SENSOR
 protected:
  std::vector<text_sensor::TextSensor *> text_sensors_;

 public:
  void register_text_sensor(text_sensor::TextSensor *obj) { this->text_sensors_.push_back(obj); }
#endif

  void set_bms_count(int bms_count);
  void set_address_base(int address_base) { address_base_ = address_base; }
  void set_replay_only(bool replay_only) { replay_only_ = replay_only; }
  void set_status_profile(const std::string &profile) {
    sp18b_fw15_profile_ = profile == "v3_sp18b_fw15";
  }
  unsigned blocking_unknown_flags() const {
    return pic_status_.blocking_reserved_flags(sp18b_fw15_profile_);
  }
  void feed_replay_frame(const std::vector<uint8_t> &frame);
  void reset_replay();
  void set_update_interval(int update_interval);
  void setup() override;
  void loop() override;
  void dump_config() override;
  bool is_valid_header();
  bool should_update(int bms_index);
  size_t get_expected_length();
  bool validate_crc(size_t length);
  void process_packet(size_t length);
  uint16_t calculate_modbus_crc(const std::deque<uint8_t> &data, size_t length);
  uint32_t received_bytes() const { return received_bytes_; }
  uint32_t valid_frames() const { return valid_frames_; }
  uint32_t crc_failures() const { return crc_failures_; }
  uint32_t unmatched_addresses() const { return unmatched_addresses_; }
  std::array<float, 16> cell_values() const {
    return {value_(cell_1_), value_(cell_2_), value_(cell_3_), value_(cell_4_),
      value_(cell_5_), value_(cell_6_), value_(cell_7_), value_(cell_8_),
      value_(cell_9_), value_(cell_10_), value_(cell_11_), value_(cell_12_),
      value_(cell_13_), value_(cell_14_), value_(cell_15_), value_(cell_16_)};
  }
  std::array<float, 6> temperature_values() const {
    return {value_(cell_temp_1_), value_(cell_temp_2_), value_(cell_temp_3_),
      value_(cell_temp_4_), value_(case_temp_), value_(power_temp_)};
  }
  bool metadata_ready() const {
    if (!(pack_fresh() && cells_fresh() && pic_fresh())) return false;
    // Plausibility checks are admission checks, not battery charge thresholds.
    auto range = [](float v, float lo, float hi) {
      return std::isfinite(v) && v >= lo && v <= hi;
    };
    if (!range(value_(pack_voltage_), 24, 72) ||
        !range(value_(current_), -2000, 2000) ||
        !range(value_(soc_), 0, 100) || !range(value_(soh_), 0, 100) ||
        !range(value_(total_capacity_), 1, 2000) ||
        !range(value_(remaining_capacity_), 0, value_(total_capacity_) + 0.01f) ||
        !range(value_(cycle_count_), 0, 65535) ||
        !range(ccl_, 0, 2000) || !range(dcl_, 0, 2000)) return false;
    float sum = 0;
    auto cells = cell_values();
    for (float v : cells) {
      if (!range(v, 1.5f, 4.5f)) return false;
      sum += v;
    }
    for (float t : temperature_values()) if (!range(t, -60, 120)) return false;
    if (!range(value_(min_cell_temp_), -60, 120) ||
        !range(value_(max_cell_temp_), -60, 120) ||
        value_(min_cell_temp_) > value_(max_cell_temp_)) return false;
    return std::fabs(sum - value_(pack_voltage_)) <= 0.5f &&
      std::fabs(*std::min_element(cells.begin(), cells.end()) - value_(min_cell_voltage_)) <= 0.05f &&
      std::fabs(*std::max_element(cells.begin(), cells.end()) - value_(max_cell_voltage_)) <= 0.05f;
  }
  float min_cell_number() const {
    if (!metadata_ready()) return NAN;
    auto values = cell_values();
    return std::min_element(values.begin(), values.end()) - values.begin() + 1;
  }
  float max_cell_number() const {
    if (!metadata_ready()) return NAN;
    auto values = cell_values();
    return std::max_element(values.begin(), values.end()) - values.begin() + 1;
  }
  bool pack_fresh() const { return pack_seen_ && uint32_t(millis() - pack_ms_) < 30000; }
  bool cells_fresh() const { return cells_seen_ && uint32_t(millis() - cells_ms_) < 30000; }
  bool pic_fresh() const { return pic_seen_ && uint32_t(millis() - pic_ms_) < 30000; }
  std::string raw_pic() const { return pic_fresh() ? raw_pic_ : "unavailable"; }
  std::string diagnostic_status() const {
    if (!pic_fresh()) return "unavailable - stale status";
    return "warnings=" + std::to_string(pic_status_.warnings()) +
      "; protections=" + std::to_string(pic_status_.protections()) +
      "; reserved=" + std::to_string(pic_status_.reserved_flags()) +
      "; accepted_fet_status=" + std::to_string(unsigned(sp18b_fw15_profile_ && pic_status_.fet_status_extension())) +
      "; blocking_unknown=" + std::to_string(blocking_unknown_flags()) +
      "; mapping=V0.3; profile=" + (sp18b_fw15_profile_ ? "v3_sp18b_fw15" : "strict");
  }
  std::string diagnostic_fets() const {
    if (!pic_fresh()) return "unavailable - stale status";
    return std::string("Charge FET ") + (pic_status_.charge_fet() ? "ON" : "OFF") +
      "; Discharge FET " + (pic_status_.discharge_fet() ? "ON" : "OFF");
  }
  const PicStatus &pic_status() const { return pic_status_; }
  SafetyPreview safety_preview() const {
    return preview_safety(pack_fresh() && cells_fresh() && pic_fresh(), pic_status_, ccl_, dcl_,
                          sp18b_fw15_profile_);
  }
  bool adapter_charge_allowed() const { return safety_preview().charge_limit > 0; }
  SafetyPreview supervised_trial_limits() const {
    return supervised_trial(safety_preview(), metadata_ready(), cell_values(), temperature_values());
  }
  SafetyPreview operating_limits() const {
    return supervised_trial(safety_preview(), metadata_ready(), cell_values(), temperature_values(), false);
  }
  bool adapter_discharge_allowed() const { return safety_preview().discharge_limit > 0; }
  bool adapter_equalizing() const {
    return pic_fresh() && !blocking_unknown_flags() &&
      !pic_status_.balancing_temperature_inhibit() && pic_status_.balancing_cells() > 0;
  }
  uint16_t adapter_errors() const {
    // 0x0800 here also denotes unsafe adapter data, not a diagnosed BMS fault.
    if (!pic_fresh()) return 0x0800;
    uint16_t mask = pic_status_.yambms_protection_mask();
    if (!pack_fresh() || !cells_fresh() || blocking_unknown_flags()) mask |= 0x0800;
    return mask;
  }
  std::string diagnostic_safety() const {
    auto preview = safety_preview();
    return preview.reason + "; CCL=" + std::to_string(preview.charge_limit) +
      "; DCL=" + std::to_string(preview.discharge_limit);
  }
  std::string diagnostic_balancing() const {
    if (!pic_fresh()) return "unavailable - stale status";
    return std::string("Module flag ") + (pic_status_.balancing_module() ? "ON" : "OFF") +
      "; static flag " + (pic_status_.static_balancing() ? "ON" : "OFF") +
      "; temperature inhibit " + (pic_status_.balancing_temperature_inhibit() ? "ON" : "OFF") +
      "; cell flags=" + std::to_string(pic_status_.balancing_cells());
  }

private:
  static float value_(const std::vector<sensor::Sensor *> &values) {
    return values.empty() || values[0] == nullptr ? NAN : values[0]->state;
  }
  void accept_byte_(uint8_t byte);
  bool replay_only_{false};
  bool sp18b_fw15_profile_{false};
  bool pack_seen_{false}, cells_seen_{false}, pic_seen_{false};
  uint32_t pack_ms_{0}, cells_ms_{0}, pic_ms_{0};
  std::string raw_pic_;
  PicStatus pic_status_;
  float ccl_{0}, dcl_{0};
  int address_base_{1};
  uint32_t received_bytes_{0};
  uint32_t valid_frames_{0};
  uint32_t crc_failures_{0};
  uint32_t unmatched_addresses_{0};
  int bms_count_;  // Variable zur Speicherung von bms_count
  uint32_t update_interval_;
  std::vector<uint32_t> last_updates_; // Timer für jedes BMS-Gerät
  //std::vector<uint8_t> buffer;
  std::deque<uint8_t> buffer;

protected:
  std::vector<sensor::Sensor *> pack_voltage_;
  std::vector<sensor::Sensor *> current_;
  std::vector<sensor::Sensor *> remaining_capacity_;
  std::vector<sensor::Sensor *> total_capacity_;
  std::vector<sensor::Sensor *> total_discharge_capacity_;
  std::vector<sensor::Sensor *> soc_;
  std::vector<sensor::Sensor *> soh_;
  std::vector<sensor::Sensor *> cycle_count_;
  std::vector<sensor::Sensor *> average_cell_voltage_;
  std::vector<sensor::Sensor *> average_cell_temp_;
  std::vector<sensor::Sensor *> max_cell_voltage_;
  std::vector<sensor::Sensor *> min_cell_voltage_;
  std::vector<sensor::Sensor *> delta_cell_voltage_;
  std::vector<sensor::Sensor *> max_cell_temp_;
  std::vector<sensor::Sensor *> min_cell_temp_;
  std::vector<sensor::Sensor *> maxdiscurt_;
  std::vector<sensor::Sensor *> maxchgcurt_;
  std::vector<sensor::Sensor *> cell_1_;
  std::vector<sensor::Sensor *> cell_2_;
  std::vector<sensor::Sensor *> cell_3_;
  std::vector<sensor::Sensor *> cell_4_;
  std::vector<sensor::Sensor *> cell_5_;
  std::vector<sensor::Sensor *> cell_6_;
  std::vector<sensor::Sensor *> cell_7_;
  std::vector<sensor::Sensor *> cell_8_;
  std::vector<sensor::Sensor *> cell_9_;
  std::vector<sensor::Sensor *> cell_10_;
  std::vector<sensor::Sensor *> cell_11_;
  std::vector<sensor::Sensor *> cell_12_;
  std::vector<sensor::Sensor *> cell_13_;
  std::vector<sensor::Sensor *> cell_14_;
  std::vector<sensor::Sensor *> cell_15_;
  std::vector<sensor::Sensor *> cell_16_;
  std::vector<sensor::Sensor *> cell_temp_1_;
  std::vector<sensor::Sensor *> cell_temp_2_;
  std::vector<sensor::Sensor *> cell_temp_3_;
  std::vector<sensor::Sensor *> cell_temp_4_;
  std::vector<sensor::Sensor *> case_temp_;
  std::vector<sensor::Sensor *> power_temp_;
  std::vector<text_sensor::TextSensor *> system_status_;
  std::vector<text_sensor::TextSensor *> active_balancing_cells_;
  std::vector<text_sensor::TextSensor *> cell_temperature_alarms_;
  std::vector<text_sensor::TextSensor *> cell_voltage_alarms_;
  std::vector<text_sensor::TextSensor *> FET_status_;
  std::vector<text_sensor::TextSensor *> active_alarms_;
  std::vector<text_sensor::TextSensor *> active_protections_;
};

}  // namespace seplos_parser
}  // namespace esphome
