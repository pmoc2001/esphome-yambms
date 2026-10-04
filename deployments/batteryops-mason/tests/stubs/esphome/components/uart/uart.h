#pragma once
#include <cstdint>
#include <deque>
namespace esphome::uart {
class UARTDevice {
 public:
  std::deque<uint8_t> incoming;
  size_t available() { return incoming.size(); }
  uint8_t read() { auto byte = incoming.front(); incoming.pop_front(); return byte; }
};
}
