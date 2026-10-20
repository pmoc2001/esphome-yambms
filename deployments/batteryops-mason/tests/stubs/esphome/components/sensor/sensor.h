#pragma once
#include <string>
#include <limits>
#include <functional>
namespace esphome::sensor {
class Sensor {
 public:
  std::string name;
  float state = std::numeric_limits<float>::quiet_NaN();
  float raw_state = std::numeric_limits<float>::quiet_NaN();
  std::string get_name() const { return name; }
  std::function<void()> on_publish;
  std::function<bool(float)> accept_publish;
  void publish_state(float value) {
    raw_state = value;
    if (accept_publish && !accept_publish(value)) return;
    state = value;
    if (on_publish) on_publish();
  }
};
}
