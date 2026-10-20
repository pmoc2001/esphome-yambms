#include <cassert>
#include <cctype>
#include <cmath>
#include <iostream>
#include "../shadow-components/seplos_parser/seplos_parser.h"

using esphome::seplos_parser::SeplosParser;

struct Bus {
  SeplosParser parser;
  std::vector<esphome::sensor::Sensor> sensors;
  std::vector<uint8_t> pack = std::vector<uint8_t>(34);
  std::vector<uint8_t> cells = std::vector<uint8_t>(52);
  std::vector<uint8_t> pic = std::vector<uint8_t>(18);

  Bus(bool initialize = true) {
    esphome::test_millis = 100;
    std::vector<std::string> names = {"pack_voltage", "current", "remaining_capacity", "total_capacity",
      "soc", "soh", "cycle_count", "min_cell_voltage", "max_cell_voltage", "min_cell_temp", "max_cell_temp"};
    for (int i = 1; i <= 16; ++i) names.push_back("cell_" + std::to_string(i));
    for (int i = 1; i <= 4; ++i) names.push_back("cell_temp_" + std::to_string(i));
    names.push_back("case_temp"); names.push_back("power_temp");
    sensors.resize(names.size());
    for (unsigned i = 0; i < names.size(); ++i) {
      sensors[i].name = "bms0 " + names[i]; parser.register_sensor(&sensors[i]);
    }
    parser.set_bms_count(1); parser.set_address_base(0);
    parser.set_status_profile("v3_sp18b_fw15_balancing");
#ifndef REPRODUCE_OLD_READER
    parser.set_transaction_tracking(true);
#endif
    parser.setup();
    word(pack, 0, 5280); word(pack, 2, 18676); word(pack, 3, 28000);
    word(pack, 5, 667); word(pack, 6, 1000); word(pack, 10, 3300); word(pack, 11, 3300);
    word(pack, 12, 2942); word(pack, 13, 2942); word(pack, 15, 180); word(pack, 16, 180);
    for (unsigned i = 0; i < 16; ++i) word(cells, i, 3300);
    for (unsigned i = 16; i < 26; ++i) word(cells, i, 2942);
    pic[15] = 0x43;
    if (initialize) { cycle(); assert(parser.metadata_ready()); }
  }
  static void word(std::vector<uint8_t> &data, unsigned index, uint16_t value) {
    data.at(index * 2) = value >> 8; data.at(index * 2 + 1) = value & 255;
  }
  std::vector<uint8_t> framed(std::vector<uint8_t> bytes) {
    std::deque<uint8_t> data(bytes.begin(), bytes.end());
    uint16_t crc = parser.calculate_modbus_crc(data, data.size());
    bytes.push_back(crc & 255); bytes.push_back(crc >> 8); return bytes;
  }
  void wire(const std::vector<uint8_t> &data) {
    parser.incoming.insert(parser.incoming.end(), data.begin(), data.end()); parser.loop();
  }
  void request(uint8_t function, uint16_t reg, uint16_t count, uint8_t address = 0) {
    wire(framed({address, function, uint8_t(reg >> 8), uint8_t(reg), uint8_t(count >> 8), uint8_t(count)}));
  }
  void response(uint8_t function, const std::vector<uint8_t> &data, uint8_t address = 0) {
    std::vector<uint8_t> frame{address, function, uint8_t(data.size())};
    frame.insert(frame.end(), data.begin(), data.end()); wire(framed(frame));
  }
  void read(uint8_t function, uint16_t reg, uint16_t count, const std::vector<uint8_t> &data) {
    request(function, reg, count); response(function, data);
  }
  void cycle() { read(4, 0x1000, 17, pack); read(4, 0x1100, 26, cells); read(1, 0x1200, 144, pic); }
  void normal() {
    assert(parser.metadata_ready());
    auto limits = parser.operating_limits();
    assert(limits.charge_limit == 56 && limits.discharge_limit == 180);
  }
  void blocked() {
    auto limits = parser.operating_limits();
    assert(limits.charge_limit == 0 && limits.discharge_limit == 0);
  }
};

int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  if (argc == 2 && std::string(argv[1]) == "--wire-replay") {
    // Input: relative monotonic milliseconds and the actual received hex bytes
    // for each capture chunk. No fabricated initial pack or added requests.
    Bus b(false);
    uint64_t t = 0, previous = 0; unsigned chunks = 0; std::string hex;
    while (std::cin >> t >> hex) {
      assert(t >= previous && t <= UINT32_MAX && hex.size() % 2 == 0);
      previous = t; esphome::test_millis = uint32_t(t);
      std::vector<uint8_t> bytes;
      for (size_t i = 0; i < hex.size(); i += 2) {
        assert(std::isxdigit(static_cast<unsigned char>(hex[i])) && std::isxdigit(static_cast<unsigned char>(hex[i+1])));
        bytes.push_back(std::stoul(hex.substr(i,2), nullptr, 16));
      }
      b.wire(bytes); ++chunks;
      auto limits = b.parser.operating_limits();
      std::string diagnostic;
#ifdef REPRODUCE_OLD_READER
      diagnostic = "legacy reader replay";
#else
      diagnostic = b.parser.diagnostic_metadata();
#endif
      std::cout << "{\"t_ms\":" << t << ",\"frames\":" << b.parser.valid_frames()
                << ",\"ready\":" << (b.parser.metadata_ready() ? "true" : "false")
                << ",\"ccl\":" << limits.charge_limit << ",\"dcl\":" << limits.discharge_limit
                << ",\"diagnostic\":\"" << diagnostic << "\"}\n";
    }
    assert(std::cin.eof() && chunks > 0);
    return 0;
  }
  {
    Bus b;
    // Actual SPA words 80..105 recorded at 2026-10-08T20:51:12.972665Z.
    const uint16_t words[] = {3400,50,30,960,150,100,50,20,28000,28000,18676,48,60,
      240,10,7,0,13,0,150,100,5600,180,65356,5,8};
    std::vector<uint8_t> settings(52);
    for (unsigned i = 0; i < 26; ++i) Bus::word(settings, i, words[i]);
    esphome::test_millis += 780;
    b.read(4, 0x1350, 26, settings);
#ifdef REPRODUCE_OLD_READER
    assert(!b.parser.metadata_ready()); b.blocked();
    assert(std::fabs(b.parser.cell_values()[1] - 0.05f) < 0.001f);
    std::cout << "REPRODUCED: settings decoded as cells; metadata OFF, limits 0/0\n";
#else
    b.normal(); assert(std::fabs(b.parser.cell_values()[1] - 3.3f) < 0.001f);
    std::cout << "PASS: recorded settings response cannot overwrite telemetry\n";
#endif
  }
  {
    Bus b;
    // Same-poll summary and cells differ after a current step at 04:45 UTC.
    Bus::word(b.pack, 0, 5393); Bus::word(b.pack, 10, 3435); Bus::word(b.pack, 11, 3352);
    b.read(4, 0x1000, 17, b.pack);
    esphome::test_millis += 240;
    const unsigned recorded_cells[] = {3356,3367,3349,3356,3347,3364,3336,3373,
                                       3320,3356,3347,3363,3344,3357,3341,3361};
    for (unsigned i = 0; i < 16; ++i) Bus::word(b.cells, i, recorded_cells[i]);
    b.read(4, 0x1100, 26, b.cells);
    esphome::test_millis += 1100;
#ifdef REPRODUCE_OLD_READER
    assert(!b.parser.metadata_ready()); b.blocked();
    std::cout << "REPRODUCED: 62mV non-atomic summary mismatch blocks after 1s\n";
#else
    b.normal();
    std::cout << "PASS: valid non-atomic measurements keep normal allowances\n";
#endif
  }
  {
    Bus b;
    // Model the actual 3s throttle: a second cell-shaped reply within 780ms
    // never reaches HA, although the rejected candidate commits it internally.
    for (auto &s : b.sensors) s.accept_publish = [](float) { return false; };
    esphome::test_millis += 780;
    std::vector<uint8_t> settings(52, 0);
    b.read(4, 0x1350, 26, settings);
    for (unsigned i = 11; i < 27; ++i) assert(std::fabs(b.sensors[i].state - 3.3f) < 0.001f);
#ifdef REPRODUCE_OLD_READER
    std::cout << "FILTER MODEL: visible cells unchanged; internal readiness=" << b.parser.metadata_ready() << '\n';
#else
    b.normal(); std::cout << "PASS: filtered display state cannot mask an internal settings overwrite\n";
#endif
  }
#ifndef REPRODUCE_OLD_READER
  {
    unsigned cases = 0;
    for (unsigned block = 0; block < 3; ++block) for (bool damage_request : {false, true}) {
      const unsigned size = damage_request ? 8 : (block == 0 ? 39 : block == 1 ? 57 : 23);
      for (unsigned at = 0; at < size; ++at) for (bool drop : {false, true}) {
        Bus b; auto before = b.parser.valid_frames();
        uint8_t fn = block == 2 ? 1 : 4;
        uint16_t reg = block == 0 ? 0x1000 : block == 1 ? 0x1100 : 0x1200;
        uint16_t count = block == 0 ? 17 : block == 1 ? 26 : 144;
        auto request = b.framed({0, fn, uint8_t(reg >> 8), uint8_t(reg), uint8_t(count >> 8), uint8_t(count)});
        const auto &payload = block == 0 ? b.pack : block == 1 ? b.cells : b.pic;
        std::vector<uint8_t> reply{0, fn, uint8_t(payload.size())};
        reply.insert(reply.end(), payload.begin(), payload.end()); reply = b.framed(reply);
        auto &damaged = damage_request ? request : reply;
        if (drop) damaged.erase(damaged.begin() + at); else damaged[at] ^= 1;
        b.wire(request); b.wire(reply);
        assert(b.parser.valid_frames() == before);
        esphome::test_millis += 2000; b.cycle(); b.normal();
        assert(b.parser.valid_frames() == before + 3); ++cases;
      }
    }
    std::cout << "PASS: " << cases << " single-byte corruption/drop cases reject and recover\n";
  }
  {
    Bus b; auto before = b.parser.valid_frames();
    std::vector<uint8_t> burst;
    auto add = [&](uint8_t fn, uint16_t reg, uint16_t count, const std::vector<uint8_t> &payload) {
      auto request = b.framed({0, fn, uint8_t(reg >> 8), uint8_t(reg), uint8_t(count >> 8), uint8_t(count)});
      std::vector<uint8_t> reply{0, fn, uint8_t(payload.size())};
      reply.insert(reply.end(), payload.begin(), payload.end()); reply = b.framed(reply);
      burst.insert(burst.end(), request.begin(), request.end()); burst.insert(burst.end(), reply.begin(), reply.end());
    };
    add(4,0x1000,17,b.pack); add(4,0x1100,26,b.cells); add(1,0x1200,144,b.pic);
    add(4,0x1700,51,std::vector<uint8_t>(102));
    add(4,0x1300,40,std::vector<uint8_t>(80)); add(4,0x1328,40,std::vector<uint8_t>(80));
    add(4,0x1350,26,std::vector<uint8_t>(52)); add(1,0x1400,80,std::vector<uint8_t>(10));
    add(4,0x1500,4,std::vector<uint8_t>(8));
    for (uint8_t byte : burst) { ++esphome::test_millis; b.wire({byte}); }
    b.normal(); assert(b.parser.valid_frames() == before + 3);
    std::cout << "PASS: complete synthetic polling/settings cycle with bytewise UART fragmentation\n";
  }
  {
    Bus b;
    auto count = b.parser.valid_frames();
    auto cells = b.cells; Bus::word(cells, 0, 3500);
    std::vector<uint8_t> payload(102, 0xff);
    auto request = b.framed({0, 4, 0x11, 0, 0, 26});
    std::vector<uint8_t> reply{0, 4, 52}; reply.insert(reply.end(), cells.begin(), cells.end());
    reply = b.framed(reply);
    std::copy(request.begin(), request.end(), payload.begin() + 10);
    std::copy(reply.begin(), reply.end(), payload.begin() + 18);
    std::vector<uint8_t> outer{0, 4, 102}; outer.insert(outer.end(), payload.begin(), payload.end());
    outer = b.framed(outer); outer.back() ^= 1;
    b.request(4, 0x1700, 51); b.wire(outer);
    assert(b.parser.valid_frames() == count);
    assert(std::fabs(b.parser.cell_values()[0] - 3.3f) < 0.001f);
    esphome::test_millis += 200; b.cycle(); b.normal();
    std::cout << "PASS: damaged outer frame cannot inject nested request/reply\n";
  }
  {
    Bus b; auto count = b.parser.valid_frames();
    // A valid PIC response can share its first eight bytes with a CRC-valid
    // read request. Prefer the complete response to the outstanding request.
    auto prefix = b.framed({0, 1, 0x12, 0, 0, 144});
    b.pic[0] = 0; b.pic[1] = 0; b.pic[2] = 144;
    b.pic[3] = prefix[6]; b.pic[4] = prefix[7];
    b.read(1, 0x1200, 144, b.pic);
    assert(b.parser.valid_frames() == count + 1);
    assert(b.parser.pic_status().warnings() > 0);
    std::cout << "PASS: response with request-shaped CRC prefix is not lost\n";
  }
  {
    Bus b; auto previous = b.parser.cell_values();
    auto invalid = b.cells; Bus::word(invalid, 0, 0);
    b.response(4, invalid); b.normal(); assert(b.parser.cell_values() == previous);
    b.read(4, 0x1350, 26, invalid); b.normal();
    b.request(4, 0x1100, 26); b.response(4, invalid, 1); b.normal();
    b.read(4, 0x1100, 26, invalid); b.blocked();
    std::cout << "PASS: only identified telemetry commits; invalid genuine telemetry blocks\n";
  }
  {
    Bus b;
    std::vector<uint8_t> identity(102, 0);
    // A complete, CRC-valid apparent cell reply embedded inside unrelated data.
    std::vector<uint8_t> inner{0, 4, 52}; inner.insert(inner.end(), b.cells.begin(), b.cells.end());
    auto framed = b.framed(inner); std::copy(framed.begin(), framed.end(), identity.begin() + 10);
    auto count = b.parser.valid_frames();
    b.read(4, 0x1700, 51, identity); b.normal(); assert(b.parser.valid_frames() == count);
    std::cout << "PASS: long unrelated reply consumed whole, not scanned as telemetry\n";
  }
  {
    Bus b; esphome::test_millis = 30100;
    b.read(4, 0x1350, 26, b.cells); b.blocked(); assert(!b.parser.cells_fresh());
    b.cycle(); b.normal();
    b.request(4, 0x1100, 26); esphome::test_millis += 2000;
    auto invalid = b.cells; Bus::word(invalid, 0, 0); b.response(4, invalid); b.normal();
    std::cout << "PASS: unrelated/late replies do not refresh telemetry; clean cycle recovers\n";
  }
  {
    for (bool summary : {false, true}) for (unsigned mv : {2890u, 3560u}) for (unsigned cell = 0; cell < 16; ++cell) {
      Bus b;
      if (summary) {
        Bus::word(b.pack, mv < 3000 ? 11 : 10, mv); b.read(4, 0x1000, 17, b.pack);
      } else { Bus::word(b.cells, cell, mv); b.read(4, 0x1100, 26, b.cells); }
      auto limits = b.parser.operating_limits();
      assert(mv < 3000 ? limits.discharge_limit == 0 : limits.charge_limit == 0);
    }
    std::cout << "PASS: unsafe extremes from either packet still stop immediately\n";
  }
  {
    for (unsigned mode = 0; mode < 4; ++mode) {
      Bus b;
      if (mode == 0) { Bus::word(b.pack, 15, 0); Bus::word(b.pack, 16, 0); b.read(4, 0x1000, 17, b.pack); }
      if (mode == 1) { b.pic[15] = 0x40; b.read(1, 0x1200, 144, b.pic); }
      if (mode == 2) { b.pic[17] = 1; b.read(1, 0x1200, 144, b.pic); }
      if (mode == 3) { Bus::word(b.cells, 24, 3182); b.read(4, 0x1100, 26, b.cells); }
      b.blocked();
    }
    std::cout << "PASS: BMS zero limits, FET stop, NTC fault and temperature guard retained\n";
  }
  {
    for (unsigned slot : {16u, 17u, 18u, 19u, 24u, 25u}) for (unsigned temp : {2731u, 3182u}) {
      Bus b; Bus::word(b.cells, slot, temp); b.read(4, 0x1100, 26, b.cells); b.blocked();
    }
    std::cout << "PASS: every temperature position retains cold/hot protection\n";
  }
  {
    Bus b;
    b.request(4, 0x1100, 26);
    b.wire(b.framed({0, 0x84, 2}));
    b.response(4, std::vector<uint8_t>(52)); b.normal();
    b.request(4, 0x1100, 26);
    auto frame = b.framed({0, 4, 52}); frame.back() ^= 1;
    frame.resize(57, 0); b.wire(frame); b.normal();
    esphome::test_millis += 200;
    b.cycle(); b.normal();
    std::cout << "PASS: exception/CRC failure cannot update data; next cycle recovers\n";
  }
  {
    Bus b;
    b.request(4, 0x1100, 26); esphome::test_millis += 1100;
    b.request(4, 0x1100, 26); b.response(4, b.cells); b.normal();
    // A valid response's payload must not be visible piecemeal to callbacks.
    unsigned callbacks = 0;
    for (auto &s : b.sensors) s.on_publish = [&] {
      for (auto value : b.parser.cell_values()) assert(std::fabs(value - 3.25f) < 0.001f);
      assert(b.parser.metadata_ready()); ++callbacks;
    };
    for (unsigned i = 0; i < 16; ++i) Bus::word(b.cells, i, 3250);
    b.read(4, 0x1100, 26, b.cells); assert(callbacks >= 22); b.normal();
    std::cout << "PASS: request retries and whole-packet publication\n";
  }
  {
    for (unsigned stale = 0; stale < 3; ++stale) {
      Bus b; esphome::test_millis = 30100;
      if (stale != 0) b.read(4, 0x1000, 17, b.pack);
      if (stale != 1) b.read(4, 0x1100, 26, b.cells);
      if (stale != 2) b.read(1, 0x1200, 144, b.pic);
      b.blocked(); assert(!b.parser.metadata_ready());
    }
    Bus b; b.parser.set_replay_only(true); b.parser.reset_replay();
    assert(!b.parser.metadata_ready()); b.blocked(); b.parser.set_replay_only(false);
    b.response(4, b.pack); b.response(4, b.cells); b.response(1, b.pic); b.blocked();
    b.cycle(); b.normal();
    esphome::test_millis = UINT32_MAX - 500; b.cycle(); b.normal();
    esphome::test_millis = 100; b.cycle(); b.normal();
    std::cout << "PASS: startup, each missing stream, replay reset and clock wrap\n";
  }
#endif
}
