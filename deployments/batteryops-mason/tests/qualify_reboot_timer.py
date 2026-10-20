"""Test resolved boot policy using ESPHome's actual restoration implementation.

Preferences are simulated. This is not a physical power-loss or live GX test.
No device connections, credentials in reports, or control writes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import esphome
from esphome.config import read_config
from esphome.core import CORE


ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "live_bench_output_enabled": ("RESTORE_DEFAULT_OFF", False),
    "yambms1_switch_charging": ("RESTORE_DEFAULT_ON", True),
    "yambms1_switch_discharging": ("RESTORE_DEFAULT_ON", True),
    "yambms1_switch_auto_ccl": ("RESTORE_DEFAULT_ON", True),
    "yambms1_switch_auto_cvl": ("RESTORE_DEFAULT_ON", True),
    "yambms1_switch_eoc_timer": ("ALWAYS_ON", True),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiled", action="store_true")
    args = parser.parse_args()
    CORE.reset()
    CORE.config_path = ROOT / "yambms-reboot-timer.yaml"
    config = read_config({})
    assert config is not None, "ESPHome configuration did not validate"
    switches = {str(s["id"]): s for s in config["switch"]}
    modes = {name: str(switches[name]["restore_mode"]) for name in EXPECTED}
    assert modes == {name: mode for name, (mode, _) in EXPECTED.items()}, modes
    callback = str(switches["yambms1_switch_auto_ccl"]["on_state"])
    assert "global_preferences->sync()" in callback

    component = Path(esphome.__file__).resolve().parent / "components/switch"
    header = (component / "switch.h").read_text()
    implementation = (component / "switch.cpp").read_text()
    constants = header[header.index("constexpr int RESTORE_MODE_ON_MASK"):
                       header.index("/** Base class")]
    start = implementation.index("optional<bool> Switch::get_initial_state_with_restore_mode()")
    end = implementation.index("\nvoid Switch::publish_state", start)
    method = implementation[start:end]
    cases = []
    for name, (mode, default) in EXPECTED.items():
        for saved in (-1, 0, 1):
            expected = default if saved == -1 or mode == "ALWAYS_ON" else bool(saved)
            cases.append((name, mode, saved, expected))
    source = """
#include <cstdint>
#include <iostream>
#include <optional>
using std::optional;
""" + constants + """
class Switch {
 public:
  SwitchRestoreMode restore_mode;
  optional<bool> saved;
  optional<bool> get_initial_state() { return saved; }
  optional<bool> get_initial_state_with_restore_mode();
};
""" + method + """
int main() {
  bool failed = false;
""" + "\n".join(
        '{ Switch s; s.restore_mode=SWITCH_' + mode + '; s.saved=' +
        ("std::nullopt" if saved == -1 else f"optional<bool>({saved})") +
        '; const auto result=s.get_initial_state_with_restore_mode();' +
        f' bool ok=result.has_value() && result.value()=={str(expected).lower()};' +
        f' std::cout << "{name}_saved_{saved} " << ok << "\\\\n"; failed|=!ok; }}'
        for name, mode, saved, expected in cases
    ).replace('"\\\\n"', '"\\n"') + "\nreturn failed ? 1 : 0;\n}\n"
    with tempfile.TemporaryDirectory(prefix="yam-reboot-policy-") as temp:
        cpp, binary = Path(temp) / "test.cpp", Path(temp) / "test"
        cpp.write_text(source)
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", str(cpp), "-o", str(binary)],
                       check=True, timeout=60)
        run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10)
    assert run.returncode == 0, run.stderr
    rows = [line.split() for line in run.stdout.splitlines()]
    assert len(rows) == len(cases) and all(value == "1" for _, value in rows), rows
    publish = implementation[implementation.index("void Switch::publish_state"):
                             implementation.index("bool Switch::assumed_state")]
    checks = {
        "resolved_switch_modes_match_policy": True,
        "auto_ccl_immediate_persistence_requested": True,
        "actual_esphome_restore_method_cases_pass": True,
        "preference_save_precedes_sync_callback": publish.index("this->rtc_.save") <
            publish.index("this->state_callback_.call"),
    }
    if args.compiled:
        build = ROOT / ".esphome/build/reboot-timer-v4"
        code = "\n".join(line for line in (build / "src/main.cpp").read_text().splitlines()
                         if not line.lstrip().startswith("//"))
        checks["compiled_modes_match"] = all(
            f"{name}->set_restore_mode(switch_::SWITCH_{mode})" in code
            for name, (mode, _) in EXPECTED.items())
        checks["timer_underflow_guard_compiled"] = all(text in code for text in (
            "if (elapsed > 0x7FFFFFFFU) return 0;",
            "std::min(elapsed, uint32_t(600 * 1000)) / 1000.0f"))
        checks["operating_identity_compiled"] = "bounded finish pause v4 reboot and diagnostic fix" in code
        checks["single_passive_reader_retained"] = not any(text in code for text in (
            "seplos_uart->set_tx_pin", "output::TurnOnAction<>(rs485_transmit_enable)",
            "seplos_link", "bms2_"))
        checks["saved_stops_and_fresh_admission_retained"] = all(text in code for text in (
            "!control_intent_saved->value()", "!live_bench_output_enabled->state",
            "!seplos_v3->metadata_ready()"))
        checks["same_current_and_voltage_limits"] = all(text in code for text in (
            "trial_cap(x, 55.2f)", "trial_cap(x, std::min(50.0f",
            "trial_cap(x, std::min(100.0f",
            "yambms1_bulk_voltage->set_initial_value(55.2f)",
            "yambms1_float_voltage->set_initial_value(53.6f)"))
        checks["finish_guards_and_pause_bounds_retained"] = all(text in code for text in (
            "if (reverse_a > 3.0f) return;", "if (reverse_ms > 60000) return;",
            "if (amp_ms > 180000.0) return;",
            "yambms1_var_finish_source_guard->value()(backstop)",
            "yambms1_var_finish_paused->value() || uint32_t(millis() - started) < duration"))
        expected = json.loads((ROOT / "tests/installed-reader-baseline.json").read_text())
        checks["installed_reader_unchanged"] = all(
            hashlib.sha256((build / "src/esphome/components/seplos_parser" / name).read_bytes()).hexdigest() == digest
            for name, digest in expected["component_sha256"].items() if name.endswith((".cpp", ".h")))
    assert all(checks.values()), checks
    report = {
        "passed": True, "live_installed": False,
        "scope": "Resolved YAML and actual ESPHome restore method with simulated preferences",
        "checks": checks, "restore_cases": len(cases), "modes": modes,
        "esphome_switch_source_sha256": hashlib.sha256(implementation.encode()).hexdigest(),
        "not_proven": ["Physical flash persistence", "Unattended power-loss recovery", "Native GX recovery"],
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
