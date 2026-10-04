#pragma once
#include <string>
#include <limits>
namespace esphome::sensor {
class Sensor {
 public:
  std::string name;
  float state = std::numeric_limits<float>::quiet_NaN();
  std::string get_name() const { return name; }
  void publish_state(float value) { state = value; }
};
}
