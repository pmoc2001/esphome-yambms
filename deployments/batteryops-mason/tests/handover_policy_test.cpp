#include <cassert>
#include <iostream>
#include "../shadow-components/seplos_parser/supervised_trial.h"

using namespace esphome::seplos_parser;

int main() {
  std::array<float,16> cells; cells.fill(3.30f);
  std::array<float,6> temps; temps.fill(21);
  auto guarded = [&](float ccl = 180, float dcl = 180, bool ready = true) {
    return supervised_trial({ccl,dcl,"BMS decision"},ready,cells,temps);
  };
  auto limits = guarded(); assert(limits.charge_limit == 35 && limits.discharge_limit == 56);
  for (float ccl : {0.0f, 5.0f, 35.0f, 180.0f}) for (float dcl : {0.0f, 8.0f, 56.0f, 180.0f}) {
    auto r = guarded(ccl,dcl);
    assert(r.charge_limit <= ccl && r.charge_limit <= 35);
    assert(r.discharge_limit <= dcl && r.discharge_limit <= 56);
  }
  assert(guarded(180,180,false).charge_limit == 0);
  for (unsigned i = 0; i < 16; ++i) {
    cells[i] = 3.55f; assert(guarded().charge_limit == 0); assert(guarded().discharge_limit == 56);
    cells[i] = 2.90f; assert(guarded().discharge_limit == 0); assert(guarded().charge_limit == 35);
    cells[i] = NAN; assert(guarded().charge_limit == 0 && guarded().discharge_limit == 0);
    cells[i] = 3.30f;
  }
  for (unsigned i = 0; i < 4; ++i) {
    temps[i] = 5; assert(guarded().charge_limit == 0 && guarded().discharge_limit == 56);
    temps[i] = 10; assert(guarded().charge_limit == 28);
    temps[i] = 0; assert(guarded().charge_limit == 0 && guarded().discharge_limit == 0);
    temps[i] = 45; assert(guarded().charge_limit == 0 && guarded().discharge_limit == 0);
    temps[i] = 21;
  }
  for (unsigned i = 0; i < 6; ++i) {
    temps[i] = NAN; assert(guarded().charge_limit == 0 && guarded().discharge_limit == 0);
    temps[i] = 21;
  }
  for (float bad : {NAN, INFINITY, -1.0f}) {
    assert(guarded(bad).charge_limit == 0 && guarded(bad).discharge_limit == 0);
    assert(trial_cap(bad,35) == 0);
  }
  assert(trial_cap(90,55.2f) == 55.2f && trial_cap(9,35) == 9);
  for (float t = -20; t <= 60; t += 0.25f) {
    temps[3] = t;
    auto r = guarded();
    assert(r.charge_limit >= 0 && r.charge_limit <= 35);
    assert(r.discharge_limit >= 0 && r.discharge_limit <= 56);
    if (t <= 5 || t >= 45) assert(r.charge_limit == 0);
    if (t <= 0 || t >= 45) assert(r.discharge_limit == 0);
  }
  std::cout << "PASS: immutable caps, 16 per-cell stops, four raw temperature limits/derating, six invalid temperatures, zero/invalid BMS limits, metadata gating\n";
}
