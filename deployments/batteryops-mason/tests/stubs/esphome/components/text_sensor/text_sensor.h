#pragma once
#include <string>
namespace esphome::text_sensor {
class TextSensor {
 public:
  std::string name;
  std::string get_name() const { return name; }
  void publish_state(const std::string &) {}
};
}
