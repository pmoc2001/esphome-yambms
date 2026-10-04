#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <string>

namespace esphome {
namespace seplos_parser {

// Manufacturer Modbus V0.3, section 4.1-4.10. Raw reserved bits remain visible.
struct PicStatus {
  std::array<uint8_t, 18> bytes{};
  bool charge_fet() const { return bytes[15] & 2; }
  bool discharge_fet() const { return bytes[15] & 1; }
  bool fet_status_extension() const { return bytes[15] & 0x40; }
  bool balancing_module() const { return bytes[16] & 1; }
  bool static_balancing() const { return bytes[16] & 2; }
  bool balancing_temperature_inhibit() const { return bytes[16] & 8; }
  uint16_t yambms_protection_mask() const {
    uint16_t mask = 0;
    if (bytes[9] & 0x22) mask |= 0x0002;   // Cell/pack OVP.
    if (bytes[9] & 0x88) mask |= 0x0004;   // Cell/pack UVP.
    if (bytes[10] & 0x02) mask |= 0x0020;  // Charge OTP.
    if (bytes[10] & 0x08) mask |= 0x0040;  // Charge UTP.
    if ((bytes[10] & 0x20) || (bytes[11] & 0x22)) mask |= 0x0008;
    if ((bytes[10] & 0x80) || (bytes[11] & 0x08)) mask |= 0x0010;
    if ((bytes[12] & 0x06) || (bytes[13] & 0x04)) mask |= 0x0100;
    if ((bytes[12] & 0x30) || (bytes[13] & 0x08)) mask |= 0x0080;
    if ((bytes[12] & 0x40) || (bytes[13] & 0x01)) mask |= 0x0400;
    if ((bytes[13] & 0x02) || (bytes[14] & 0x48) || bytes[17]) mask |= 0x0800;
    return mask;
  }
  unsigned balancing_cells() const {
    static constexpr uint8_t masks[18] = {0,0,0,0,0,0,255,255,0,0,0,0,0,0,0,0,0,0};
    return count(masks);
  }
  unsigned warnings() const {
    static constexpr uint8_t masks[18] = {
      255,255,255,255,255,255,0,0,0,0x55,0x55,0x15,0x09,0,0x14,0,0x14,0
    };
    return count(masks);
  }
  unsigned protections() const {
    static constexpr uint8_t masks[18] = {
      0,0,0,0,0,0,0,0,0,0xaa,0xaa,0x2a,0x76,0x0f,0x48,0,0,0xff
    };
    return count(masks);
  }
  unsigned reserved_flags() const {
    static constexpr uint8_t masks[18] = {
      0,0,0,0,0,0,0,0,0xc0,0,0,0x80,0x80,0xf0,0xa3,0xf0,0xe0,0
    };
    return count(masks);
  }
  unsigned blocking_reserved_flags(bool sp18b_fw15_profile = false) const {
    // PIC and EIC share the FET-state table. Only the observed 0x40 status
    // extension is non-fault for this qualified profile; never clear raw data.
    return reserved_flags() - unsigned(sp18b_fw15_profile && fet_status_extension());
  }
 private:
  unsigned count(const uint8_t *masks) const {
    unsigned result = 0;
    for (unsigned i = 0; i < bytes.size(); ++i) {
      uint8_t bits = bytes[i] & masks[i];
      while (bits) { result += bits & 1; bits >>= 1; }
    }
    return result;
  }
};

// Shared permission decision; the caller separately controls merge/CAN admission.
struct SafetyPreview {
  float charge_limit{0}, discharge_limit{0};
  std::string reason;
};
inline SafetyPreview preview_safety(bool fresh, const PicStatus &pic, float ccl, float dcl,
                                   bool sp18b_fw15_profile = false) {
  if (!fresh) return {0, 0, "blocked: incomplete or stale data"};
  if (!std::isfinite(ccl) || !std::isfinite(dcl) || ccl < 0 || dcl < 0)
    return {0, 0, "blocked: invalid current limits"};
  if (pic.blocking_reserved_flags(sp18b_fw15_profile))
    return {0, 0, "blocked: undocumented status bits"};
  if (pic.protections()) return {0, 0, "blocked: protection asserted"};
  return {pic.charge_fet() ? ccl : 0, pic.discharge_fet() ? dcl : 0,
          "reported limits and documented FET permissions"};
}
}  // namespace seplos_parser
}  // namespace esphome
