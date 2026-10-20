#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include "../shadow-components/seplos_parser/seplos_parser.h"

using esphome::seplos_parser::SeplosParser;

struct Fixture {
  SeplosParser parser;
  std::array<esphome::sensor::Sensor, 33> sensors;
  std::vector<uint8_t> pack = std::vector<uint8_t>(34);
  std::vector<uint8_t> cells = std::vector<uint8_t>(52);
  std::vector<uint8_t> pic = std::vector<uint8_t>(18);

  Fixture(bool start = true) {
    esphome::test_millis = 100;
    const char *names[] = {"pack_voltage", "current", "remaining_capacity", "total_capacity",
      "soc", "soh", "cycle_count", "min_cell_voltage", "max_cell_voltage",
      "min_cell_temp", "max_cell_temp", "cell_1", "cell_2", "cell_3", "cell_4",
      "cell_5", "cell_6", "cell_7", "cell_8", "cell_9", "cell_10", "cell_11",
      "cell_12", "cell_13", "cell_14", "cell_15", "cell_16", "cell_temp_1",
      "cell_temp_2", "cell_temp_3", "cell_temp_4", "case_temp", "power_temp"};
    for (unsigned i = 0; i < sensors.size(); ++i) {
      sensors[i].name = std::string("bms0 ") + names[i];
      parser.register_sensor(&sensors[i]);
    }
    parser.set_bms_count(1); parser.set_address_base(0);
    parser.set_status_profile("v3_sp18b_fw15_balancing"); parser.setup();
    word(pack, 2, 12040); word(pack, 3, 28000); word(pack, 5, 430);
    word(pack, 6, 1000); word(pack, 7, 20);
    word(pack, 15, 180); word(pack, 16, 180);
    pic[15] = 0x43;
    levels(3300);
    if (start) { send_pack(); send_cells(); send_pic(); assert(parser.metadata_ready()); }
  }
  static void word(std::vector<uint8_t> &data, unsigned index, uint16_t value) {
    data.at(2 * index) = value >> 8; data.at(2 * index + 1) = value & 255;
  }
  void levels(unsigned mv, unsigned kelvin_tenths = 2942) {
    word(pack, 0, std::lround(mv * 16.0 / 10.0));
    word(pack, 10, mv); word(pack, 11, mv);
    word(pack, 12, kelvin_tenths); word(pack, 13, kelvin_tenths);
    for (unsigned i = 0; i < 16; ++i) word(cells, i, mv);
    for (unsigned i = 16; i < 26; ++i) word(cells, i, kelvin_tenths);
  }
  void feed(uint8_t function, const std::vector<uint8_t> &payload, bool corrupt = false) {
    std::deque<uint8_t> frame{0, function, uint8_t(payload.size())};
    frame.insert(frame.end(), payload.begin(), payload.end());
    const uint16_t crc = parser.calculate_modbus_crc(frame, frame.size());
    frame.push_back(crc & 255); frame.push_back((crc >> 8) ^ (corrupt ? 1 : 0));
    parser.incoming.insert(parser.incoming.end(), frame.begin(), frame.end()); parser.loop();
  }
  void send_pack() { feed(4, pack); }
  void send_cells() { feed(4, cells); }
  void send_pic() { feed(1, pic); }
  void normal() {
    const auto limits = parser.operating_limits();
    assert(parser.metadata_ready());
    assert(limits.charge_limit == 56 && limits.discharge_limit == 180);
  }
  void blocked() {
    const auto limits = parser.operating_limits();
    assert(limits.charge_limit == 0 && limits.discharge_limit == 0);
  }
};

void test(const char *name, const std::function<void()> &body) {
  body(); std::cout << "PASS: " << name << '\n';
}

int main(int argc, char **argv) {
  std::cout << std::unitbuf;
  if (argc == 2 && std::string(argv[1]) == "--live-sample") {
    Fixture f(false);
    std::array<float, 35> values;
    for (float &v : values) { std::cin >> v; assert(std::cin && std::isfinite(v)); }
    std::string pic; std::cin >> pic; assert(pic.size() == 36);
    for (unsigned i = 0; i < 18; ++i) f.pic[i] = std::stoul(pic.substr(2 * i, 2), nullptr, 16);
    Fixture::word(f.pack, 0, std::lround(values[0] * 100));
    Fixture::word(f.pack, 1, uint16_t(int16_t(std::lround(values[1] * 100))));
    Fixture::word(f.pack, 2, std::lround(values[2] * 100));
    Fixture::word(f.pack, 3, std::lround(values[3] * 100));
    Fixture::word(f.pack, 5, std::lround(values[4] * 10));
    Fixture::word(f.pack, 6, std::lround(values[5] * 10));
    Fixture::word(f.pack, 7, std::lround(values[6]));
    Fixture::word(f.pack, 10, std::lround(values[8] * 1000));
    Fixture::word(f.pack, 11, std::lround(values[7] * 1000));
    Fixture::word(f.pack, 12, std::lround((values[10] + 273.15f) * 10));
    Fixture::word(f.pack, 13, std::lround((values[9] + 273.15f) * 10));
    Fixture::word(f.pack, 15, std::lround(values[34]));
    Fixture::word(f.pack, 16, std::lround(values[33]));
    for (unsigned i = 0; i < 16; ++i) Fixture::word(f.cells, i, std::lround(values[11 + i] * 1000));
    for (unsigned i = 0; i < 6; ++i)
      Fixture::word(f.cells, i < 4 ? 16 + i : 24 + i - 4, std::lround((values[27 + i] + 273.15f) * 10));
    f.send_pack(); f.send_cells(); f.send_pic();
    assert(f.parser.metadata_ready());
    const auto limits = f.parser.operating_limits();
    assert(limits.charge_limit > 0 && limits.discharge_limit > 0);
    std::cout << "PASS: reconstructed live decoded sample; source CCL=" << limits.charge_limit
              << "; source DCL=" << limits.discharge_limit << '\n';
    return 0;
  }
  test("load-on: new cells then pack after 256ms", [] {
    Fixture f; f.levels(3250); esphome::test_millis = 10100;
    f.send_cells(); f.normal(); esphome::test_millis += 256;
    f.send_pack(); f.normal();
  });
  test("load-off: new pack then cells after 148ms", [] {
    Fixture f; f.levels(3250); f.send_pack(); f.send_cells();
    f.levels(3300); esphome::test_millis = 10100;
    f.send_pack(); f.normal(); esphome::test_millis += 148;
    f.send_cells(); f.normal();
  });
  test("persistent mismatch blocks at 1000ms; duplicate frames cannot extend wait", [] {
    Fixture f; f.levels(3250); esphome::test_millis = 10100; f.send_cells();
    for (unsigned offset = 0; offset < 1000; offset += 200) {
      esphome::test_millis = 10100 + offset; f.send_cells(); f.normal();
    }
    esphome::test_millis = 11100; f.send_cells(); f.blocked();
    assert(!f.parser.metadata_ready()); f.send_pack(); f.normal();
  });
  test("startup never inherits a grace allowance", [] {
    Fixture f(false); f.send_pack(); f.levels(3250); f.send_cells(); f.send_pic();
    assert(!f.parser.metadata_ready()); f.blocked(); f.send_pack(); f.normal();
  });
  test("late counterpart needs a fresh matching pair", [] {
    Fixture f; f.levels(3250); esphome::test_millis = 10100; f.send_cells();
    esphome::test_millis = 11100; f.send_pack(); f.blocked();
    f.send_cells(); f.normal();
  });
  test("invalid cells, pack metadata and temperatures block immediately", [] {
    for (unsigned mode = 0; mode < 3; ++mode) {
      Fixture f; esphome::test_millis = 10100;
      if (mode == 0) { Fixture::word(f.cells, 4, 0); f.send_cells(); }
      if (mode == 1) { Fixture::word(f.pack, 5, 1001); f.send_pack(); }
      if (mode == 2) { Fixture::word(f.cells, 24, 65535); f.send_cells(); }
      f.blocked(); assert(!f.parser.metadata_ready());
    }
  });
  test("new zero BMS allowances are immediate during pair wait", [] {
    Fixture f; f.levels(3250); esphome::test_millis = 10100;
    Fixture::word(f.pack, 15, 0); Fixture::word(f.pack, 16, 0);
    f.send_pack(); assert(f.parser.metadata_ready()); f.blocked();
  });
  test("PIC faults and FET stops are immediate during pair wait", [] {
    for (unsigned mode = 0; mode < 4; ++mode) {
      Fixture f; f.levels(3250); esphome::test_millis = 10100; f.send_cells();
      if (mode == 0) f.pic[15] = 0x40;
      if (mode == 1) f.pic[17] = 1;
      if (mode == 2) f.pic[16] = 0x80;
      if (mode == 3) f.pic[9] = 2;
      f.send_pic(); f.blocked();
    }
  });
  test("unsafe cell extremes block from either packet without waiting", [] {
    for (unsigned mv : {2890u, 3560u}) for (bool pack_first : {false, true}) {
      Fixture f; f.levels(mv); esphome::test_millis = 10100;
      if (pack_first) f.send_pack(); else f.send_cells();
      const auto limits = f.parser.operating_limits();
      if (mv < 2900) assert(limits.discharge_limit == 0);
      else assert(limits.charge_limit == 0);
    }
  });
  test("unsafe temperature stops/derating from either packet without waiting", [] {
    for (unsigned kelvin : {2731u, 2781u, 2831u, 3182u}) for (bool pack_first : {false, true}) {
      Fixture f; f.levels(3300, kelvin); esphome::test_millis = 10100;
      if (pack_first) f.send_pack(); else f.send_cells();
      const auto limits = f.parser.operating_limits();
      if (kelvin <= 2781 || kelvin >= 3182) assert(limits.charge_limit == 0);
      if (kelvin <= 2731 || kelvin >= 3182) assert(limits.discharge_limit == 0);
      if (kelvin == 2831) assert(limits.charge_limit > 0 && limits.charge_limit < 28.1f);
    }
  });
  test("stale pack, cells or PIC never gets a grace allowance", [] {
    for (unsigned stale = 0; stale < 3; ++stale) {
      Fixture f; esphome::test_millis = 30100;
      if (stale != 0) f.send_pack(); if (stale != 1) f.send_cells(); if (stale != 2) f.send_pic();
      assert(!f.parser.metadata_ready()); f.blocked();
    }
  });
  test("packet callbacks see the entire decoded frame, not partial sensor states", [] {
    Fixture f; unsigned callbacks = 0;
    for (auto &sensor : f.sensors) sensor.on_publish = [&] {
      assert(f.parser.metadata_ready());
      for (float v : f.parser.cell_values()) assert(std::fabs(v - 3.25f) < 0.001f);
      ++callbacks;
    };
    f.levels(3250); esphome::test_millis = 10100; f.send_cells();
    esphome::test_millis += 200; f.send_pack(); f.normal(); assert(callbacks > 20);
  });
  test("CRC rejection does not refresh or extend a pair wait", [] {
    Fixture f; f.levels(3250); esphome::test_millis = 10100; f.send_cells();
    esphome::test_millis = 10500; f.feed(4, f.pack, true);
    esphome::test_millis = 11100; f.blocked(); assert(f.parser.crc_failures() > 0);
  });
  test("pair deadline and freshness survive millisecond clock wrap", [] {
    Fixture f; esphome::test_millis = UINT32_MAX - 500;
    f.send_pack(); f.send_cells(); f.send_pic(); f.normal();
    esphome::test_millis = UINT32_MAX - 100; f.levels(3250); f.send_pack(); f.normal();
    esphome::test_millis = 155; f.send_cells(); f.normal();
    esphome::test_millis = 30155; f.blocked();
  });
  test("repeated load transitions never zero normal allowances", [] {
    Fixture f;
    for (unsigned i = 0; i < 50; ++i) {
      esphome::test_millis = 1000 + i * 5000; f.levels(i % 2 ? 3300 : 3250);
      if (i % 2) f.send_pack(); else f.send_cells(); f.normal();
      esphome::test_millis += 300;
      if (i % 2) f.send_cells(); else f.send_pack(); f.send_pic(); f.normal();
    }
  });
}
