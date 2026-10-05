#include <cassert>
#include <iostream>
#include "../shadow-components/seplos_parser/seplos_parser.h"

using esphome::seplos_parser::SeplosParser;

void inject(SeplosParser &parser, uint8_t function, std::vector<uint8_t> data) {
  std::deque<uint8_t> frame{0, function, uint8_t(data.size())};
  frame.insert(frame.end(), data.begin(), data.end());
  uint16_t crc = 0xffff;
  for (auto byte : frame) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
  }
  frame.push_back(crc & 255); frame.push_back(crc >> 8);
  parser.incoming.insert(parser.incoming.end(), frame.begin(), frame.end());
  while (parser.available()) parser.loop();
}

void limits(SeplosParser &parser, uint16_t ccl, uint16_t dcl) {
  std::vector<uint8_t> pack(34);
  pack[30] = dcl >> 8; pack[31] = dcl & 255;
  pack[32] = ccl >> 8; pack[33] = ccl & 255;
  inject(parser, 4, pack);
}

int main() {
  SeplosParser parser;
  parser.set_bms_count(1); parser.set_address_base(0); parser.setup();
  auto blocked = [&]() {
    auto result = parser.safety_preview();
    assert(result.charge_limit == 0 && result.discharge_limit == 0);
  };
  blocked();  // Startup: no cached permissions may be trusted.
  limits(parser, 180, 180); blocked();
  inject(parser, 4, std::vector<uint8_t>(52)); blocked();
  std::vector<uint8_t> pic(18); pic[15] = 3;
  inject(parser, 1, pic);
  assert(parser.safety_preview().charge_limit == 180);
  assert(parser.safety_preview().discharge_limit == 180);
  limits(parser, 0, 180);
  assert(parser.safety_preview().charge_limit == 0);
  assert(parser.safety_preview().discharge_limit == 180);
  limits(parser, 180, 0);
  assert(parser.safety_preview().charge_limit == 180);
  assert(parser.safety_preview().discharge_limit == 0);
  limits(parser, 0, 0); blocked();
  limits(parser, 180, 180);
  for (unsigned fets = 0; fets < 4; ++fets) {
    pic[15] = fets; inject(parser, 1, pic);
    assert(parser.safety_preview().charge_limit == ((fets & 2) ? 180 : 0));
    assert(parser.safety_preview().discharge_limit == ((fets & 1) ? 180 : 0));
  }
  const unsigned protection_coils[] = {
    0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x57,0x59,0x5b,0x5d,
    0x61,0x62,0x64,0x65,0x66,0x68,0x69,0x6a,0x6b,0x73,0x76,
    0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f
  };
  for (auto coil : protection_coils) {
    pic.assign(18, 0); pic[15] = 3;
    pic[coil / 8] |= 1 << (coil % 8);
    inject(parser, 1, pic); blocked();
  }
  pic.assign(18, 0); pic[15] = 0x43;
  inject(parser, 1, pic); blocked();
  assert(parser.safety_preview().reason == "blocked: undocumented status bits");
  pic[15] = 3; inject(parser, 1, pic);
  esphome::test_millis = 29000;
  inject(parser, 4, std::vector<uint8_t>(52)); inject(parser, 1, pic);
  esphome::test_millis = 30000; blocked();  // Only pack expired.
  limits(parser, 180, 180);
  assert(parser.safety_preview().charge_limit == 180);
  esphome::test_millis = 59000; blocked();  // Cells and status expired.
  limits(parser, 180, 180); blocked();
  inject(parser, 4, std::vector<uint8_t>(52)); blocked(); // PIC alone missing.
  inject(parser, 1, pic);
  assert(parser.safety_preview().charge_limit == 180);
  SeplosParser restarted;
  restarted.set_bms_count(1); restarted.set_address_base(0); restarted.setup();
  assert(restarted.safety_preview().charge_limit == 0);
  assert(restarted.safety_preview().discharge_limit == 0);
  assert(restarted.diagnostic_balancing() == "unavailable - stale status");
  std::cout << "PASS: diagnostic-only safety preview: stop requests, 30 protections, FET-off, unknown bits, partial data loss, restart\n";
}
