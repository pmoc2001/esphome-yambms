#include <cassert>
#include <cmath>
#include <iostream>
#include "../shadow-components/seplos_parser/seplos_parser.h"

using namespace esphome::seplos_parser;

int main() {
  PicStatus pic;
  pic.bytes = {0,0,0,0,0,0,8,0,2,0,0,0,0,0,0,0x43,0x21,0};
  const auto original = pic.bytes;
  assert(pic.reserved_flags() == 2);
  assert(pic.blocking_reserved_flags() == 2);
  assert(pic.blocking_reserved_flags(true) == 1); // Legacy profile is unchanged.
  assert(pic.blocking_reserved_flags(false, true) == 2);
  assert(pic.blocking_reserved_flags(true, true) == 0);
  assert(preview_safety(true, pic, 180, 180, true).charge_limit == 0);
  const auto allowed = preview_safety(true, pic, 180, 180, true, true);
  assert(allowed.charge_limit == 180 && allowed.discharge_limit == 180);
  assert(pic.bytes == original && pic.yambms_protection_mask() == 0);
  assert(pic.balancing_module() && pic.balancing_cells() == 1);

  for (unsigned fets = 0; fets < 4; ++fets) {
    pic.bytes = original; pic.bytes[15] = 0x40 | fets;
    const auto result = preview_safety(true, pic, 35, 100, true, true);
    assert(result.charge_limit == ((fets & 2) ? 35 : 0));
    assert(result.discharge_limit == ((fets & 1) ? 100 : 0));
  }
  pic.bytes = original;
  for (auto limits : {std::array<float, 2>{0,180}, {180,0}, {0,0}}) {
    const auto result = preview_safety(true, pic, limits[0], limits[1], true, true);
    assert(result.charge_limit == limits[0] && result.discharge_limit == limits[1]);
  }
  assert(preview_safety(false, pic, 180, 180, true, true).charge_limit == 0);
  for (float bad : {NAN, INFINITY, -1.0f}) {
    assert(preview_safety(true, pic, bad, 180, true, true).charge_limit == 0);
    assert(preview_safety(true, pic, 180, bad, true, true).discharge_limit == 0);
  }
  const unsigned protections[] = {
    0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x57,0x59,0x5b,0x5d,
    0x61,0x62,0x64,0x65,0x66,0x68,0x69,0x6a,0x6b,0x73,0x76,
    0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
  };
  for (auto coil : protections) {
    pic.bytes = original; pic.bytes[coil / 8] |= 1 << (coil % 8);
    const auto result = preview_safety(true, pic, 180, 180, true, true);
    assert(result.charge_limit == 0 && result.discharge_limit == 0);
    assert(pic.yambms_protection_mask() != 0);
  }
  const unsigned warnings[] = {0x48,0x4a,0x4c,0x4e,0x50,0x52,0x54,0x56,
    0x58,0x5a,0x5c,0x60,0x63,0x72,0x74,0x82,0x84};
  for (auto coil : warnings) {
    pic.bytes = original; pic.bytes[coil / 8] |= 1 << (coil % 8);
    assert(pic.warnings() == 1); // No warning bit is erased by compatibility.
  }
  unsigned unknown = 0;
  for (unsigned coil = 0; coil < 144; ++coil) {
    PicStatus bit; bit.bytes[coil / 8] = 1 << (coil % 8);
    if (!bit.reserved_flags() || coil == 0x7e || coil == 0x85) continue;
    pic.bytes = original; pic.bytes[coil / 8] |= 1 << (coil % 8);
    assert(pic.blocking_reserved_flags(true, true) == 1);
    const auto result = preview_safety(true, pic, 180, 180, true, true);
    assert(result.charge_limit == 0 && result.discharge_limit == 0);
    ++unknown;
  }
  SeplosParser parser;
  parser.set_bms_count(1); parser.set_address_base(0); parser.setup();
  parser.set_status_profile("v3_sp18b_fw15_balancing");
  assert(parser.adapter_errors() == 0x0800);
  assert(!parser.adapter_charge_allowed() && !parser.adapter_discharge_allowed());
  std::cout << "PASS: recorded PIC; strict/legacy unchanged; 4 FET combinations; "
            << "30 protections; 17 warnings; " << unknown
            << " other reserved bits; zero/invalid limits; raw bytes; startup/stale\n";
}
