#include <cassert>
#include <iostream>
#include "../shadow-components/seplos_parser/seplos_parser.h"

using esphome::seplos_parser::SeplosParser;

std::deque<uint8_t> response(uint8_t function, size_t count, const std::vector<uint8_t> &payload = {}) {
  std::deque<uint8_t> bytes{0, function, uint8_t(count)};
  for (size_t i = 0; i < count; i++) bytes.push_back(i < payload.size() ? payload[i] : 0);
  uint16_t crc = 0xffff;
  for (auto byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
  }
  bytes.push_back(crc & 255); bytes.push_back(crc >> 8);
  return bytes;
}

void feed(SeplosParser &parser, std::deque<uint8_t> data) {
  parser.incoming.insert(parser.incoming.end(), data.begin(), data.end());
  while (parser.available()) parser.loop();
}

int main() {
  SeplosParser parser;
  parser.set_bms_count(1); parser.set_address_base(0); parser.setup();
  assert(!parser.pack_fresh() && !parser.cells_fresh() && !parser.pic_fresh());
  assert(parser.diagnostic_fets() == "unavailable - stale status");
  assert(parser.raw_pic() == "unavailable");
  feed(parser, response(4, 34));
  assert(parser.pack_fresh() && !parser.cells_fresh() && !parser.pic_fresh());
  feed(parser, response(4, 52));
  feed(parser, response(1, 18));
  assert(parser.pack_fresh() && parser.cells_fresh() && parser.pic_fresh());
  assert(parser.raw_pic() == std::string(36, '0'));
  assert(parser.diagnostic_fets() == "Charge FET OFF; Discharge FET OFF");
  std::vector<uint8_t> pic(18, 0);
  pic[15] = 0x43;  // Actual sample: both FET bits plus undocumented bit 6.
  feed(parser, response(1, 18, pic));
  assert(parser.diagnostic_fets() == "Charge FET ON; Discharge FET ON");
  assert(parser.pic_status().reserved_flags() == 1);
  pic[9] = 2;  // Cell overvoltage protection in a CRC-valid simulated reply.
  feed(parser, response(1, 18, pic));
  assert(parser.pic_status().protections() == 1);
  esphome::test_millis = 30000;
  assert(!parser.pack_fresh() && !parser.cells_fresh() && !parser.pic_fresh());
  assert(parser.raw_pic() == "unavailable");
  assert(parser.diagnostic_status() == "unavailable - stale status");
  feed(parser, response(4, 34));
  assert(parser.pack_fresh() && !parser.cells_fresh() && !parser.pic_fresh());
  auto damaged = response(1, 18); damaged.back() ^= 1;
  feed(parser, damaged);
  assert(!parser.pic_fresh());
  // Unsigned elapsed-time arithmetic must also expire correctly across wrap.
  esphome::test_millis = UINT32_MAX - 10000;
  feed(parser, response(4, 34));
  esphome::test_millis = 10000;
  assert(parser.pack_fresh());
  esphome::test_millis = 25000;
  assert(!parser.pack_fresh());
  SeplosParser replay;
  replay.set_bms_count(1); replay.set_address_base(0);
  replay.set_replay_only(true); replay.setup();
  auto recorded = response(1, 18, pic);
  replay.feed_replay_frame(std::vector<uint8_t>(recorded.begin(), recorded.end()));
  assert(replay.pic_fresh() && replay.pic_status().reserved_flags() == 1);
  assert(replay.pic_status().protections() == 1);
  replay.incoming = response(4, 34);
  replay.loop();
  assert(!replay.pack_fresh() && !replay.incoming.empty());
  replay.reset_replay();
  assert(!replay.pic_fresh() && replay.raw_pic() == "unavailable");
  // Injection into a physical UART parser must be ignored.
  auto valid_before = parser.valid_frames();
  parser.feed_replay_frame(std::vector<uint8_t>(recorded.begin(), recorded.end()));
  assert(parser.valid_frames() == valid_before);
  std::cout << "PASS: per-channel freshness, startup unknown, CRC rejection, raw PIC expiry, clock wrap\n";
}
