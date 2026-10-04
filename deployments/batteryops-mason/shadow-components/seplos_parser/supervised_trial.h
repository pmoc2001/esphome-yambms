#pragma once
#include <algorithm>
#include "pic_status.h"

namespace esphome {
namespace seplos_parser {

// Shared unsmoothed protections; legacy trials additionally clamp current.
inline SafetyPreview supervised_trial(SafetyPreview bms, bool metadata_ready,
                                     const std::array<float, 16> &cells,
                                     const std::array<float, 6> &temperatures,
                                     bool commissioning_caps = true) {
  if (!metadata_ready) return {0, 0, "trial blocked: incomplete/invalid metadata"};
  for (float v : cells) if (!std::isfinite(v)) return {0, 0, "trial blocked: invalid cell"};
  for (float t : temperatures) if (!std::isfinite(t)) return {0, 0, "trial blocked: invalid temperature"};
  if (!std::isfinite(bms.charge_limit) || !std::isfinite(bms.discharge_limit) ||
      bms.charge_limit < 0 || bms.discharge_limit < 0)
    return {0, 0, "trial blocked: invalid permission limits"};
  if (commissioning_caps) {
    bms.charge_limit = std::min(bms.charge_limit, 35.0f);
    bms.discharge_limit = std::min(bms.discharge_limit, 56.0f);
  }
  const float cold = *std::min_element(temperatures.begin(), temperatures.begin() + 4);
  const float hot = *std::max_element(temperatures.begin(), temperatures.begin() + 4);
  if (cold <= 5 || hot >= 45) {
    bms.charge_limit = 0;
    bms.reason = "trial charge blocked: cell temperature outside (5,45)C";
  } else {
    const float warm_charge_limit = 56.0f * std::min(1.0f, (cold - 5.0f) / 10.0f);
    bms.charge_limit = std::min(bms.charge_limit, warm_charge_limit);
  }
  if (cold <= 0 || hot >= 45) {
    bms.discharge_limit = 0;
    bms.reason += "; trial discharge blocked: temperature outside (0,45)C";
  }
  if (*std::max_element(cells.begin(), cells.end()) >= 3.55f) {
    bms.charge_limit = 0;
    bms.reason += "; trial charge blocked: highest cell >=3.55V";
  }
  if (*std::min_element(cells.begin(), cells.end()) <= 2.90f) {
    bms.discharge_limit = 0;
    bms.reason += "; trial discharge blocked: lowest cell <=2.90V";
  }
  return bms;
}

inline float trial_cap(float value, float maximum) {
  return std::isfinite(value) ? std::max(0.0f, std::min(value, maximum)) : 0.0f;
}

}  // namespace seplos_parser
}  // namespace esphome
