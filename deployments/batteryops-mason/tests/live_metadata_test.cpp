#include <cassert>
#include <cmath>
#include <iostream>
#include "../shadow-components/seplos_parser/seplos_parser.h"

using esphome::seplos_parser::SeplosParser;

void feed(SeplosParser &p, uint8_t function, const std::vector<uint8_t> &payload) {
  std::deque<uint8_t> frame{0, function, uint8_t(payload.size())};
  frame.insert(frame.end(), payload.begin(), payload.end());
  uint16_t crc = p.calculate_modbus_crc(frame, frame.size());
  frame.push_back(crc & 255); frame.push_back(crc >> 8);
  p.incoming.insert(p.incoming.end(), frame.begin(), frame.end());
  p.loop();
}

void word(std::vector<uint8_t> &data, unsigned index, uint16_t value) {
  data.at(index * 2) = value >> 8; data.at(index * 2 + 1) = value & 255;
}

int main() {
  SeplosParser p;
  const char *names[] = {"pack_voltage", "current", "remaining_capacity", "total_capacity",
    "soc", "soh", "cycle_count", "min_cell_voltage", "max_cell_voltage",
    "min_cell_temp", "max_cell_temp", "cell_1", "cell_2", "cell_3", "cell_4",
    "cell_5", "cell_6", "cell_7", "cell_8", "cell_9", "cell_10", "cell_11",
    "cell_12", "cell_13", "cell_14", "cell_15", "cell_16", "cell_temp_1",
    "cell_temp_2", "cell_temp_3", "cell_temp_4", "case_temp", "power_temp"};
  std::array<esphome::sensor::Sensor, 33> sensors;
  for (unsigned i = 0; i < sensors.size(); ++i) {
    sensors[i].name = std::string("bms0 ") + names[i]; p.register_sensor(&sensors[i]);
  }
  p.set_bms_count(1); p.set_address_base(0); p.setup();
  assert(!p.metadata_ready() && std::isnan(p.min_cell_number()));
  std::vector<uint8_t> pack(34), cells(52), pic(18);
  word(pack, 0, 5226); word(pack, 1, uint16_t(int16_t(-930)));
  word(pack, 2, 9800); word(pack, 3, 28000); word(pack, 5, 350);
  word(pack, 6, 1000); word(pack, 7, 35);
  word(pack, 10, 3270); word(pack, 11, 3258);
  word(pack, 12, 2952); word(pack, 13, 2942);
  word(pack, 15, 180); word(pack, 16, 180);
  for (unsigned i = 0; i < 16; ++i) word(cells, i, 3267);
  word(cells, 3, 3258); word(cells, 13, 3270);
  for (unsigned i = 16; i < 20; ++i) word(cells, i, 2942 + i - 16);
  word(cells, 24, 2952); word(cells, 25, 2962);
  pic[15] = 0x43;
  feed(p, 4, pack); feed(p, 4, cells); feed(p, 1, pic);
  assert(p.metadata_ready());
  assert(p.min_cell_number() == 4 && p.max_cell_number() == 14);
  assert(std::fabs(sensors[1].state + 9.3f) < 0.001f);
  assert(std::fabs(p.temperature_values()[0] - 21.05f) < 0.001f);
  assert(!p.adapter_charge_allowed() && !p.adapter_discharge_allowed());
  assert(p.adapter_errors() & 0x0800); // Unknown stays visible and blocking.
  word(cells, 3, 0); feed(p, 4, cells);
  assert(!p.metadata_ready() && std::isnan(p.min_cell_number()));
  word(cells, 3, 3258); feed(p, 4, cells); assert(p.metadata_ready());
  word(cells, 24, 65535); feed(p, 4, cells); assert(!p.metadata_ready());
  word(cells, 24, 2952); feed(p, 4, cells); assert(p.metadata_ready());
  word(pack, 5, 1001); feed(p, 4, pack); assert(!p.metadata_ready());
  word(pack, 5, 350); feed(p, 4, pack); assert(p.metadata_ready());
  word(pack, 0, 6000); feed(p, 4, pack);
  assert(p.metadata_ready()); // Only cross-packet disagreement gets a bounded wait.
  esphome::test_millis += 1000;
  assert(!p.metadata_ready());
  word(pack, 0, 5226); word(pack, 15, 0); word(pack, 16, 0);
  feed(p, 4, pack); assert(!p.metadata_ready()); // The counterpart is now too old to pair.
  feed(p, 4, cells); assert(p.metadata_ready()); // Zero allowances are legitimate, not missing.
  sensors[12].state = NAN; assert(!p.metadata_ready());
  feed(p, 4, cells); assert(p.metadata_ready());
  esphome::test_millis = 30000;
  assert(!p.metadata_ready() && std::isnan(p.max_cell_number()));
  feed(p, 4, pack); assert(!p.metadata_ready());
  feed(p, 4, cells); assert(!p.metadata_ready());
  feed(p, 1, pic); assert(p.metadata_ready());
  std::cout << "PASS: real metadata, signed current, all cells/temperatures, indices, invalid data, zero limits, expiry, partial recovery, unknown retained\n";
}
