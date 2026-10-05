#include <cassert>
#include <cmath>
#include <iostream>
#include "../shadow-components/seplos_parser/seplos_parser.h"

using namespace esphome::seplos_parser;

int main() {
  PicStatus pic;
  pic.bytes[15] = 0x43;
  assert(pic.reserved_flags() == 1 && pic.fet_status_extension());
  assert(pic.blocking_reserved_flags() == 1);
  assert(pic.blocking_reserved_flags(true) == 0);
  assert(preview_safety(true, pic, 180, 180).charge_limit == 0);
  assert(preview_safety(true, pic, 180, 180, true).charge_limit == 180);
  assert(pic.bytes[15] == 0x43); // Original byte is never normalized or erased.
  for (unsigned fets = 0; fets < 4; ++fets) {
    pic = {}; pic.bytes[15] = 0x40 | fets;
    auto result = preview_safety(true, pic, 180, 180, true);
    assert(result.charge_limit == ((fets & 2) ? 180 : 0));
    assert(result.discharge_limit == ((fets & 1) ? 180 : 0));
  }
  pic.bytes[15] = 0x43;
  for (auto limits : {std::array<float, 2>{0, 180}, {180, 0}, {0, 0}}) {
    auto result = preview_safety(true, pic, limits[0], limits[1], true);
    assert(result.charge_limit == limits[0] && result.discharge_limit == limits[1]);
  }
  const unsigned protection_coils[] = {
    0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x57,0x59,0x5b,0x5d,
    0x61,0x62,0x64,0x65,0x66,0x68,0x69,0x6a,0x6b,0x73,0x76,
    0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
  };
  for (unsigned coil : protection_coils) {
    pic = {}; pic.bytes[15] = 0x43; pic.bytes[coil / 8] |= 1 << (coil % 8);
    assert(pic.protections() == 1);
    auto result = preview_safety(true, pic, 180, 180, true);
    assert(result.charge_limit == 0 && result.discharge_limit == 0);
    assert(pic.yambms_protection_mask() != 0);
  }
  unsigned unclassified = 0;
  for (unsigned coil = 0; coil < 144; ++coil) {
    PicStatus single;
    single.bytes[coil / 8] = 1 << (coil % 8);
    if (!single.reserved_flags() || coil == 0x7e) continue;
    pic = single; pic.bytes[15] |= 0x43;
    assert(pic.blocking_reserved_flags(true) >= 1);
    auto result = preview_safety(true, pic, 180, 180, true);
    assert(result.charge_limit == 0 && result.discharge_limit == 0);
    ++unclassified;
  }
  pic = {}; pic.bytes[15] = 0x43;
  assert(preview_safety(false, pic, 180, 180, true).charge_limit == 0);
  for (float bad : {NAN, INFINITY, -1.0f}) {
    assert(preview_safety(true, pic, bad, 180, true).charge_limit == 0);
    assert(preview_safety(true, pic, 180, bad, true).discharge_limit == 0);
  }
  SeplosParser parser;
  parser.set_bms_count(1); parser.set_address_base(0); parser.setup();
  parser.set_status_profile("v3_sp18b_fw15");
  assert(!parser.adapter_charge_allowed() && !parser.adapter_discharge_allowed());
  assert(parser.adapter_errors() == 0x0800); // Startup remains blocked.
  std::cout << "PASS: profile-only FET status, all FET combinations, zero limits, 30 protections, "
            << unclassified << " other unknown bits, invalid limits, startup/stale data\n";
}
