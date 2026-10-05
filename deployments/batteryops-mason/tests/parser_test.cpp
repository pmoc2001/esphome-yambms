#include <cassert>
#include <cmath>
#include <iostream>
#ifdef SHADOW_PARSER
#include "../shadow-components/seplos_parser/seplos_parser.h"
#else
#include "../components/seplos_parser/seplos_parser.h"
#endif

using esphome::seplos_parser::SeplosParser;
using esphome::sensor::Sensor;

std::deque<uint8_t> frame(const std::vector<uint16_t> &words, uint8_t address = 1) {
  std::deque<uint8_t> data{address, 4, uint8_t(words.size() * 2)};
  for (auto word : words) {
    data.push_back(word >> 8);
    data.push_back(word & 255);
  }
  uint16_t crc = 0xffff;
  for (auto byte : data) {
    crc ^= byte;
    for (int i = 0; i < 8; i++) crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
  }
  data.push_back(crc & 255);
  data.push_back(crc >> 8);
  return data;
}

void feed(SeplosParser &parser, const std::deque<uint8_t> &data) {
  parser.incoming.insert(parser.incoming.end(), data.begin(), data.end());
  while (parser.available()) parser.loop();
}

int main() {
  SeplosParser parser;
  parser.set_bms_count(1);
  Sensor voltage, current, soc, limit, cell;
  voltage.name = "bms0 pack_voltage";
  current.name = "bms0 current";
  soc.name = "bms0 soc";
  limit.name = "bms0 maxchgcurt";
  cell.name = "bms0 cell_4";
  for (auto *sensor : {&voltage, &current, &soc, &limit, &cell}) parser.register_sensor(sensor);
  parser.setup();
  std::vector<uint16_t> pack(17, 0);
  pack[0] = 5296; pack[1] = uint16_t(int16_t(-835)); pack[5] = 538; pack[16] = 85;
  feed(parser, frame(pack));
  assert(std::abs(voltage.state - 52.96f) < 0.001f);
  assert(std::abs(current.state + 8.35f) < 0.001f);
  assert(std::abs(soc.state - 53.8f) < 0.001f);
  assert(limit.state == 85);
  pack.push_back(0);
  feed(parser, frame(pack));
  assert(limit.state == 85);
  std::vector<uint16_t> cells(26, 3300);
  cells[3] = 3312;
  feed(parser, frame(cells));
  assert(std::abs(cell.state - 3.312f) < 0.0001f);
  auto corrupt = frame(pack); corrupt.back() ^= 1;
  feed(parser, corrupt);
  assert(parser.crc_failures() > 0);
  feed(parser, frame(pack, 2));
  assert(parser.unmatched_addresses() == 1);
  assert(parser.valid_frames() == 4);
  assert(parser.received_bytes() == 219);
  SeplosParser standalone;
  standalone.set_bms_count(1);
  standalone.set_address_base(0);
  standalone.register_sensor(&voltage);
  standalone.register_sensor(&cell);
  standalone.setup();
  voltage.state = NAN; cell.state = NAN;
  pack.resize(17);
  feed(standalone, frame(pack, 0));
  feed(standalone, frame(cells, 0));
  assert(std::abs(voltage.state - 52.96f) < 0.001f);
  assert(std::abs(cell.state - 3.312f) < 0.0001f);
  assert(standalone.valid_frames() == 2);
  SeplosParser zero_limits;
  zero_limits.set_bms_count(1); zero_limits.set_address_base(0);
  Sensor ccl, dcl;
  ccl.name = "bms0 maxchgcurt"; dcl.name = "bms0 maxdiscurt";
  zero_limits.register_sensor(&ccl); zero_limits.register_sensor(&dcl);
  zero_limits.setup();
  pack[15] = 180; pack[16] = 0;
  feed(zero_limits, frame(pack, 0));
  assert(ccl.state == 0 && dcl.state == 180);
  pack[15] = 0; pack[16] = 180;
  feed(zero_limits, frame(pack, 0));
  assert(ccl.state == 180 && dcl.state == 0);
  pack[16] = 0;
  feed(zero_limits, frame(pack, 0));
  assert(ccl.state == 0 && dcl.state == 0);
  std::cout << "PASS: address bases 0/1, 17/18-register pack, cells, signed current, CRC rejection, address mismatch, receive counters\n";
}
