"""Compile the actual diagnostic lambda; never connect to or write a device."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import yaml


ROOT = Path(__file__).resolve().parents[1]


class Loader(yaml.SafeLoader):
    pass


Loader.add_multi_constructor(
    "!", lambda loader, tag, node: loader.construct_scalar(node)
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=ROOT / "yambms-finish-pause.yaml")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    doc = yaml.load(args.source.read_text(), Loader=Loader)
    sensor = next(s for s in doc["sensor"] if s["name"] == "Finishing Qualified Time")
    profile = yaml.load((ROOT / "yambms-top-charge-trial.yaml").read_text(), Loader=Loader)
    confirmation_seconds = int(profile["substitutions"]["yambms_cutoff_timer"])
    assert confirmation_seconds == 600, "Review the boundary tests if the policy changes"
    body = sensor["lambda"].replace("${yambms_cutoff_timer}", str(confirmation_seconds))
    body = re.sub(r"\bid\((\w+)\)", r"\1", body)
    source = r'''
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
bool yambms1_var_cutoff_timer_armed;
uint32_t yambms1_var_finish_check_ms, yambms1_var_cutoff_timer_started;
float qualified_seconds() {
''' + body + r'''
}
struct Case {
  const char *name;
  bool armed;
  uint32_t checked, started;
  float expected;
};
int main() {
  const Case cases[] = {
    {"disarmed_zero", false, 0, 0, 0},
    {"disarmed_old_timer", false, 800000, 100000, 0},
    {"armed_same_tick", true, 100000, 100000, 0},
    {"armed_one_tick_after_check", true, 100000, 100001, 0},
    {"armed_after_check_larger_delay", true, 100000, 100500, 0},
    {"armed_after_check_across_wrap", true, 0xFFFFFFFFU, 0, 0},
    {"first_valid_check", true, 101001, 100001, 1},
    {"partial_second", true, 100501, 100001, 0.5f},
    {"before_deadline", true, 699999, 100000, 599.999f},
    {"at_deadline", true, 700000, 100000, 600},
    {"after_deadline_clamped", true, 710000, 100000, 600},
    {"real_millis_wrap", true, 499, 0xFFFFFE0BU, 1},
    {"real_wrap_before_deadline", true, 599499, 0xFFFFFE0BU, 600},
    {"pause_before_shift", true, 110000, 100000, 10},
    {"pause_after_shift", true, 115000, 105000, 10},
    {"continued_pause_after_shift", true, 120000, 110000, 10},
    {"resume_after_pause", true, 121000, 110000, 11},
    {"pause_across_wrap", true, 499, 0xFFFFDAE3U, 10},
    {"cancelled_timer", false, 121000, 110000, 0},
    {"balancing_rearmed_after_check", true, 121000, 121001, 0},
  };
  bool failed = false;
  for (const auto &c : cases) {
    yambms1_var_cutoff_timer_armed = c.armed;
    yambms1_var_finish_check_ms = c.checked;
    yambms1_var_cutoff_timer_started = c.started;
    const float actual = qualified_seconds();
    const bool pass = std::isfinite(actual) && std::abs(actual - c.expected) < 0.001f;
    std::cout << c.name << ' ' << pass << ' ' << actual << ' ' << c.expected << '\n';
    failed |= !pass;
  }
  return failed ? 1 : 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="yam-finish-diagnostic-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(source)
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", str(cpp), "-o", str(binary)],
                       check=True, timeout=60)
        run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10)
    results = []
    for line in run.stdout.splitlines():
        name, passed, actual, expected = line.split()
        results.append({"scenario": name, "passed": passed == "1",
                        "actual": float(actual), "expected": float(expected)})
    passed = run.returncode == 0 and len(results) == 20 and all(r["passed"] for r in results)
    report = {"scope": "Actual YAML diagnostic lambda, mocked clock fields; no controls",
              "source": str(args.source.resolve()),
              "source_sha256": hashlib.sha256(args.source.read_bytes()).hexdigest(),
              "confirmation_seconds": confirmation_seconds,
              "passed": passed, "results": results, "stderr": run.stderr}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if passed else 1)


if __name__ == "__main__":
    main()
