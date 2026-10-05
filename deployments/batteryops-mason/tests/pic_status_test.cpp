#include <cassert>
#include <iostream>
#include "../shadow-components/seplos_parser/pic_status.h"

using esphome::seplos_parser::PicStatus;

int main() {
  PicStatus status;
  assert(!status.charge_fet() && !status.discharge_fet());
  assert(status.warnings() == 0 && status.protections() == 0);
  // Independent coil-address fixtures taken from the manufacturer's tables.
  const unsigned protection_coils[] = {
    0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x57,0x59,0x5b,0x5d,
    0x61,0x62,0x64,0x65,0x66,0x68,0x69,0x6a,0x6b,0x73,0x76,
    0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
  };
  for (auto coil : protection_coils) {
    status = {};
    status.bytes[coil / 8] = 1 << (coil % 8);
    assert(status.protections() == 1 && status.warnings() == 0);
  }
  const unsigned warning_coils[] = {
    0x48,0x4a,0x4c,0x4e,0x50,0x52,0x54,0x56,0x58,0x5a,0x5c,0x60,0x63,0x72,0x74,0x82,0x84
  };
  for (auto coil : warning_coils) {
    status = {};
    status.bytes[coil / 8] = 1 << (coil % 8);
    assert(status.warnings() == 1 && status.protections() == 0);
  }
  for (unsigned i = 0; i < 48; ++i) {
    status = {}; status.bytes[i / 8] = 1 << (i % 8);
    assert(status.warnings() == 1);
  }
  for (unsigned fets = 0; fets < 4; ++fets) {
    status = {}; status.bytes[15] = fets;
    assert(status.charge_fet() == bool(fets & 2));
    assert(status.discharge_fet() == bool(fets & 1));
  }
  status = {}; status.bytes[16] = 3; status.bytes[8] = 2;
  assert(status.warnings() == 0 && status.protections() == 0);
  assert(status.balancing_module() && status.static_balancing());
  status.bytes[6] = 1; status.bytes[7] = 128;
  assert(status.balancing_cells() == 2);
  status.bytes[15] = 0x43;  // Bit 6 remains reserved even in V0.3.
  assert(status.reserved_flags() == 1);
  std::cout << "PASS: 30 protection bits, 65 warning bits, FET combinations, balancing, reserved status\n";
}
