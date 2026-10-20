"""Execute source-extracted YamBMS lambdas with local fake ESPHome entities.

No network, device writes or firmware changes. Timer scheduling and entities
are mocks; charge/merge/request bodies come directly from the package YAML.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import yaml
import argparse

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--adapter', action='store_true')
parser.add_argument('--handover', action='store_true')
parser.add_argument('--restart-safe', action='store_true')
parser.add_argument('--operating', action='store_true')
parser.add_argument('--finish-session', action='store_true',
                    help='Exercise guarded completion timers, with the deployed 600-second setting')
parser.add_argument('--balancing-status', action='store_true',
                    help='Replay the qualified balancing extension through every operating scenario')
parser.add_argument('--output', type=Path, help='Write evidence without replacing a prior qualification')
parser.add_argument('--charge-replay', type=Path,
                    help='Offline normalized recording to replay with AutoCCL ON and OFF')
parser.add_argument('--bounded-pause', action='store_true',
                    help='Exercise bounded reversal qualification and real raw-parser callbacks')
parser.add_argument('--finish-recording', type=Path,
                    help='Replay normalized native finishing records without controlling devices')
args = parser.parse_args()
if args.bounded_pause and not args.finish_session:
    parser.error('--bounded-pause requires --finish-session')
if args.finish_recording and not (args.bounded_pause and args.operating and args.output):
    parser.error('--finish-recording requires --bounded-pause --operating --output')
if args.charge_replay and not (args.operating and args.finish_session and args.output):
    parser.error('--charge-replay requires --operating --finish-session --output')
if args.balancing_status and not args.operating:
    parser.error('--balancing-status requires --operating')
if args.operating:
    args.restart_safe = True
if args.restart_safe:
    args.handover = True
if args.handover:
    args.adapter = True

class Loader(yaml.SafeLoader):
    pass

Loader.add_multi_constructor('!', lambda loader, tag, node: loader.construct_scalar(node)
                             if isinstance(node, yaml.ScalarNode) else
                             loader.construct_sequence(node) if isinstance(node, yaml.SequenceNode) else
                             loader.construct_mapping(node))

paths = {
    'core': ROOT / 'yam-packages/yambms/yambms_core.yaml',
    'merge': ROOT / 'yam-packages/bms/bms_combine.yaml',
    'float': ROOT / 'yam-packages/yambms/yambms_auto_float.yaml',
    'ccl': ROOT / 'yam-packages/yambms/yambms_auto_ccl.yaml',
}
if args.adapter:
    paths['adapter'] = ROOT / 'yam-packages/bms/bms_sensors_SEPLOS_V3_RS485_bms_full.yaml'
    paths['balancer'] = ROOT / 'yam-packages/bms/bms_base_balancer.yaml'
if args.operating:
    paths['temperature'] = ROOT / 'yam-packages/yambms/yambms_auto_temp_ccl_dcl.yaml'
docs = {key: yaml.load(path.read_text(), Loader=Loader) for key, path in paths.items()}
subs = {'yambms_id': '1', 'bms_id': '1', 'yambms_max_requested_discharge_current': '85',
        'charge_a_factor_curve_end': '2.0', 'charge_a_factor_curve_shape': '1.8',
        'seplos_parser_id': 'seplos_v3', 'yambms_eoc_timer': '30',
        'yambms_cutoff_timer': '600' if args.finish_session else '60'}
if args.operating:
    subs['yambms_max_requested_discharge_current'] = '180'

def expand(text):
    text = re.sub(r'\$\{([^}]+)\}', lambda match: subs[match[1]], text)
    return re.sub(r'\bid\((\w+)\)', r'\1', text)

def entity_lambda(doc, section, suffix):
    return next(item['lambda'] for item in doc[section] if str(item.get('id', '')).endswith(suffix))

bodies = {
    'eligible': ('bool', entity_lambda(docs['merge'], 'binary_sensor', '_can_be_combined')),
    'merge': ('void', docs['merge']['interval'][0]['then'][0]['lambda']),
    'core': ('void', docs['core']['interval'][0]['then'][0]['lambda']),
    'float_voltage': ('float', entity_lambda(docs['float'], 'sensor', '_auto_float_v')),
    'auto_ccl': ('void', docs['ccl']['interval'][0]['then'][0]['lambda']),
    'eoc': ('void', next(item for item in docs['core']['script']
                       if item['id'].endswith('_script_eoc'))['then'][0]['lambda']),
}
for name in ('finish_check', 'finish_cancel'):
    bodies[name] = ('void', next(item for item in docs['core']['script']
                                if item['id'].endswith('_script_' + name))['then'][0]['lambda'])
for name in ('eoc', 'cutoff'):
    delay = next(item for item in docs['core']['script']
                 if item['id'].endswith('_script_' + name + '_timer'))['then'][0]['delay']
    bodies[name + '_delay'] = ('uint32_t', delay)
if args.adapter:
    for key, section, suffix, kind in [
        ('adapter_online', 'binary_sensor', '_online_status', 'bool'),
        ('adapter_charge', 'binary_sensor', '_charging_allowed', 'bool'),
        ('adapter_discharge', 'binary_sensor', '_discharging_allowed', 'bool'),
        ('adapter_equalizing', 'binary_sensor', '_bms_equalizing', 'bool'),
        ('adapter_errors', 'sensor', '_yambms_errors_bitmask', 'float'),
        ('adapter_ccl', 'sensor', '_max_charge_current', 'float'),
        ('adapter_dcl', 'sensor', '_max_discharge_current', 'float'),
    ]:
        bodies[key] = (kind, entity_lambda(docs['adapter'], section, suffix))
    bodies['merged_equalizing'] = ('bool', entity_lambda(docs['balancer'], 'binary_sensor', '_equalizing'))
if args.handover:
    paths['handover'] = ROOT / 'yambms-handover-trial.yaml'
    trial = yaml.load(paths['handover'].read_text(), Loader=Loader)
    live = yaml.load((ROOT / 'yambms-live-data-bench.yaml').read_text(), Loader=Loader)
    bodies['eligible'] = ('bool', entity_lambda(live, 'binary_sensor', '_can_be_combined'))
    for key, section, suffix, kind in [
        ('adapter_charge', 'binary_sensor', '_charging_allowed', 'bool'),
        ('adapter_discharge', 'binary_sensor', '_discharging_allowed', 'bool'),
        ('adapter_ccl', 'sensor', '_max_charge_current', 'float'),
        ('adapter_dcl', 'sensor', '_max_discharge_current', 'float'),
    ]:
        bodies[key] = (kind, entity_lambda(trial, section, suffix))
    for key, suffix in [('filter_cvl', '_requested_charge_voltage'),
                        ('filter_ccl', '_requested_charge_current'),
                        ('filter_dcl', '_requested_discharge_current')]:
        item = next(item for item in trial['sensor'] if item['id'].endswith(suffix))
        bodies[key] = ('float', next(f['lambda'] for f in item['filters'] if 'lambda' in f))
if args.restart_safe:
    paths['restart'] = ROOT / 'yambms-restart-safe.yaml'
    docs['restart'] = yaml.load(paths['restart'].read_text(), Loader=Loader)
    bodies['eligible'] = ('bool', entity_lambda(docs['restart'], 'binary_sensor', '_can_be_combined'))
    bodies['commit'] = ('void', docs['restart']['switch'][0]['on_state'][0]['lambda'])
if args.operating:
    paths['operating'] = ROOT / 'yambms-operating.yaml'
    operating = yaml.load(paths['operating'].read_text(), Loader=Loader)
    subs['yambms_discharging_rate_table'] = operating['substitutions']['yambms_discharging_rate_table']
    bodies['temperature_dcl'] = ('void', docs['temperature']['interval'][0]['then'][1]['lambda'])
    for key, section, suffix, kind in [
        ('adapter_charge', 'binary_sensor', '_charging_allowed', 'bool'),
        ('adapter_discharge', 'binary_sensor', '_discharging_allowed', 'bool'),
        ('adapter_ccl', 'sensor', '_max_charge_current', 'float'),
        ('adapter_dcl', 'sensor', '_max_discharge_current', 'float'),
    ]:
        bodies[key] = (kind, entity_lambda(operating, section, suffix))
    for key, suffix in [('filter_ccl', '_requested_charge_current'),
                        ('filter_dcl', '_requested_discharge_current')]:
        item = next(item for item in operating['sensor'] if item['id'].endswith(suffix)
                    and isinstance(item.get('filters'),list))
        bodies[key] = ('float', next(f['lambda'] for f in item['filters'] if 'lambda' in f))
if args.finish_session and args.operating:
    paths['finish_session'] = ROOT / 'yambms-finish-session-fix.yaml'
    finish = yaml.load(paths['finish_session'].read_text(), Loader=Loader)
    bodies['finish_source_init'] = ('void', finish['esphome']['on_boot'][0]['then'][0]['lambda'])
if args.charge_replay:
    paths['cvl'] = ROOT / 'yam-packages/yambms/yambms_auto_cvl.yaml'
    paths['charge_ceiling'] = ROOT / 'yambms-charge-ceiling-50a.yaml'
    docs['cvl'] = yaml.load(paths['cvl'].read_text(), Loader=Loader)
    ceiling = yaml.load(paths['charge_ceiling'].read_text(), Loader=Loader)
    bodies['auto_cvl'] = ('float', entity_lambda(docs['cvl'], 'sensor', 'auto_cvl'))
    current = next(item for item in ceiling['sensor']
                   if item['id'].endswith('_requested_charge_current')
                   and isinstance(item.get('filters'), list))
    bodies['filter_ccl'] = ('float', next(item['lambda'] for item in current['filters']
                                        if 'lambda' in item))
code = {key: expand(value[1]) for key, value in bodies.items()}
ids = set(re.findall(r'\bid\(([^)]+)\)', '\n'.join(value[1] for value in bodies.values())))
names = {expand('id(' + value + ')') for value in ids}
globals_ = {}
for doc in docs.values():
    for item in doc.get('globals', []):
        globals_[expand(item['id'])] = (item['type'], item.get('initial_value') or '{}')
text_ids = {expand(item['id']) for item in docs['core']['text_sensor'] if 'id' in item}
declarations = []
for name in sorted(names):
    if name == 'seplos_v3':
        declarations.append('esphome::seplos_parser::SeplosParser seplos_v3;')
    elif name in globals_:
        kind, initial = globals_[name]
        declarations.append(f'{kind} {name} = {expand(initial)};')
    elif name == 'my_time':
        declarations.append('Clock my_time;')
    elif 'script_' in name:
        declarations.append(f'Script {name};')
    elif name == 'global_update_fault':
        declarations.append('auto global_update_fault = [](auto... args) {};')
    elif name in text_ids:
        declarations.append(f'Text {name};')
    else:
        declarations.append(f'Entity {name};')

prefix = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
using std::min; using std::max; using std::isnan;
#define ESP_LOGD(...)
#define ESP_LOGW(...)
#define ESP_LOGI(...)
#define ESP_LOGE(...)
#ifdef ADAPTER_MODE
#include "seplos_parser/seplos_parser.h"
uint32_t &now_ms = esphome::test_millis;
#else
static uint32_t now_ms = 0;
#endif
uint32_t millis() { return now_ms; }
struct Entity {
  float state = 0, raw_state = NAN; bool present = true; unsigned selection = 0;
  bool has_state() const { return present; }
  float get_raw_state() const { return raw_state; }
  void publish_state(float value) { state = value; present = true; }
  void turn_off() { state = 0; }
  std::optional<unsigned> active_index() { return selection; }
};
struct Text {
  std::string state; bool present = true;
  bool has_state() const { return present; }
  void publish_state(std::string value) { state = value; }
};
struct Clock {
  struct Stamp { time_t timestamp; };
  Stamp utcnow() { return {1791020000 + now_ms / 1000}; }
};
struct Script {
  bool running = false; uint32_t deadline = 0, delay_ms = 0;
  int attempt = 0; bool backstop = false;
  std::function<void(int, bool)> callback;
  std::function<uint32_t()> delay_function;
  bool is_running() const { return running; }
  void execute(int value = 0, bool kind = false) {
    if (running) return;
    if (delay_function) delay_ms = delay_function();
    attempt = value; backstop = kind; running = true; deadline = now_ms + delay_ms;
    if (delay_ms == 0) { if (callback) callback(attempt, backstop); running = false; }
  }
  void stop() { running = false; }
  void poll() {
    if (running && int32_t(now_ms - deadline) >= 0) {
      running = false; if (callback) callback(attempt, backstop);
    }
  }
};
#ifdef RESTART_MODE
struct TestPreferences {
  bool succeeds = true;
  unsigned calls = 0;
  bool sync() { ++calls; return succeeds; }
} test_preferences;
namespace esphome { auto *global_preferences = &test_preferences; }
#endif
'''
functions = '\n'.join(f'{kind} run_{key}({"float x" if key.startswith("filter_") else "int attempt, bool backstop" if key == "eoc" else ""}) {{\n{code[key]}\n}}'
                      for key, (kind, _) in bodies.items())
fixture = (ROOT / ('tests/yambms_adapter_scenarios.cpp' if args.adapter
                   else 'tests/yambms_logic_scenarios.cpp')).read_text()
if args.finish_session:
    fixture = fixture.split('int main(', 1)[0] + (ROOT / 'tests/yambms_finish_scenarios.cpp').read_text()
if args.charge_replay:
    fixture = fixture.split('int main(', 1)[0] + (ROOT / 'tests/yambms_charge_replay.cpp').read_text()
if args.bounded_pause:
    fixture = fixture.split('int main(', 1)[0] + (ROOT / 'tests/yambms_pause_scenarios.cpp').read_text()
if args.finish_recording:
    fixture = fixture.split('int main(', 1)[0]
    fixture += (ROOT / 'tests/yambms_charge_replay.cpp').read_text().split('int main(', 1)[0]
    fixture += (ROOT / 'tests/yambms_finish_recording.cpp').read_text()
with tempfile.TemporaryDirectory(prefix='batteryops-yam-logic-') as temporary:
    directory = Path(temporary)
    source = directory / 'test.cpp'
    source.write_text(prefix + '\n'.join(declarations) + '\n' + functions + '\n' + fixture)
    binary = directory / 'test'
    command = ['g++', '-std=c++17', '-fsanitize=address,undefined', '-g']
    if args.adapter:
        command += ['-DADAPTER_MODE', '-I' + str(ROOT / 'tests/stubs'),
                    '-I' + str(ROOT / 'shadow-components'),
                    str(ROOT / 'shadow-components/seplos_parser/seplos_parser.cpp')]
    if args.handover:
        command += ['-DHANDOVER_MODE']
    if args.restart_safe:
        command += ['-DRESTART_MODE']
    if args.operating:
        command += ['-DOPERATING_MODE']
    if args.balancing_status:
        command += ['-DBALANCE_STATUS_MODE']
    if args.finish_session:
        command += ['-DFINISH_SESSION_MODE']
    subprocess.run(command + [str(source), '-o', str(binary)], check=True)
    scenarios = ['normal', 'zero_ccl', 'zero_dcl', 'charge_off', 'disconnect',
                 'startup', 'cutoff_float', 'no_float', 'auto_ccl_stop', 'missing_measurement',
                 'zero_after_charge', 'discharge_off', 'early_soc_100', 'balancing_timer',
                 'invalid_limit', 'infinite_limit', 'both_zero', 'float_charge_off']
    if args.adapter:
        scenarios = ['normal', 'zero_ccl', 'zero_dcl', 'both_zero', 'charge_off',
                     'discharge_off', 'protections', 'warning_only', 'reserved',
                     'balancing', 'temperature_inhibit', 'stale_pack', 'stale_cells',
                     'stale_pic', 'startup', 'crc_rejected']
        scenarios += ['v3_fet_extension', 'v3_fet_extension_protection',
                      'v3_fet_extension_other_unknown', 'v3_fet_extension_charge_off']
        scenarios += ['v3_fet_extension_' + name for name in
                      ['protections', 'startup', 'crc_rejected', 'stale_pack',
                       'stale_cells', 'stale_pic', 'zero_ccl', 'zero_dcl', 'both_zero']]
    if args.handover:
        scenarios = [name for name in scenarios if name.startswith('v3_fet_extension')]
        scenarios += ['trial_cold', 'trial_hot', 'trial_derating', 'trial_cell_high',
                      'trial_cell_low', 'trial_invalid_metadata', 'trial_zero_charge_switch',
                      'trial_zero_discharge_switch', 'trial_recovery']
    if args.restart_safe:
        scenarios += ['restart_not_saved', 'restart_manual_stop', 'restart_commit_ok',
                      'restart_commit_failure', 'restart_return']
    if args.operating:
        scenarios += ['operating_normal', 'operating_bms_reduction', 'operating_output_caps',
                      'operating_discharge_cold', 'operating_invalid_temperature', 'operating_legacy_caps']
        scenarios += ['operating_temperature_warm', 'operating_temperature_cool',
                      'operating_temperature_hot', 'operating_temperature_missing']
    if args.finish_session:
        if args.operating:
            scenarios = ['raw_' + kind + '_' + fault for kind in ('short', 'backstop') for fault in
                         ('normal', 'warning', 'accepted_status', 'ccl_zero', 'charge_fet', 'protection', 'unknown', 'stale_pack',
                          'stale_cells', 'stale_pic', 'master_off', 'unsaved', 'high_cell',
                          'cold', 'hot', 'discharge', 'qualification', 'balancing_at_expiry')]
        else:
            scenarios = [kind + '_' + fault for kind in ('short', 'backstop') for fault in
                         ('normal', 'charge_off', 'permission', 'offline', 'discharge',
                          'zero_limit', 'invalid', 'qualification', 'guard', 'force_float',
                          'cancel_recovery', 'exact_expiry')]
            scenarios += ['old_callback', 'balancer_restarts_short', 'backstop_disabled',
                          'float_disabled', 'millis_wrap', 'attempt_wrap', 'duplicate_callback',
                          'timestamp_preserved', 'no_shunt_guarantee', 'short_balancing_at_expiry',
                          'backstop_disabled_at_expiry', 'short_limit_zero_at_expiry',
                          'backstop_limit_zero_at_expiry', 'bulk_balancing_status']
    results = []
    if args.bounded_pause:
        scenarios = ['short_pause_resume', 'backstop_pause_resume', 'negative_at_deadline',
                     'exact_three_amps', 'over_three_amps', 'exact_sixty_seconds',
                     'over_sixty_seconds', 'cumulative_budget', 'duplicate_checks',
                     'no_start_while_discharging', 'low_voltage_during_pause',
                     'balancing_stays_on', 'balancing_restarts', 'stop_during_pause',
                     'old_callback_after_pause_cancel', 'millis_wrap_pause',
                     'repeated_reversals_resume', 'backstop_disabled_during_pause']
        if args.operating:
            scenarios += ['raw_negative_before_merge', 'raw_positive_merged_negative',
                          'raw_charge_stop_during_pause', 'raw_stale_during_pause',
                          'raw_high_cell_during_pause', 'raw_balancing_before_expiry',
                          'raw_invalid_current', 'raw_voltage_drop_during_pause']
        if args.finish_recording:
            scenarios = ['recording_timer_first', 'recording_core_first']
    if args.charge_replay:
        scenarios = ['on_core_first', 'off_core_first', 'on_auto_first', 'off_auto_first']
    for scenario in scenarios:
        command = [str(binary), scenario]
        if args.charge_replay:
            command.append(str(args.charge_replay.resolve()))
        if args.finish_recording:
            command.append(str(args.finish_recording.resolve()))
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        results.append({'scenario': scenario, 'exit_code': result.returncode,
                        'output': result.stdout.strip(), 'error': result.stderr.strip()})
    report = {'source_sha256': {key: hashlib.sha256(path.read_bytes()).hexdigest()
                               for key, path in paths.items()},
              'scope': 'Actual source lambdas; fake entities, timers and clock; no CAN transport',
              'results': results}
    if args.charge_replay:
        report['recording_sha256'] = hashlib.sha256(args.charge_replay.read_bytes()).hexdigest()
        report['fixture_sha256'] = hashlib.sha256((ROOT / 'tests/yambms_charge_replay.cpp').read_bytes()).hexdigest()
        report['scope'] = ('Source-extracted decision replay; mocked entities, scheduler and timers; '
                           'reconstructed Modbus frames; no battery physics or CAN transport')
    if args.bounded_pause:
        report['fixture_sha256'] = hashlib.sha256((ROOT / 'tests/yambms_pause_scenarios.cpp').read_bytes()).hexdigest()
    if args.finish_recording:
        report['recording_sha256'] = hashlib.sha256(args.finish_recording.read_bytes()).hexdigest()
        report['recording_fixture_sha256'] = hashlib.sha256((ROOT / 'tests/yambms_finish_recording.cpp').read_bytes()).hexdigest()
        report['scope'] = 'Recorded native trajectory through actual source and real parser; zero-order hold between samples; not battery-physics simulation'
    if args.adapter:
        report['parser_sha256'] = {name: hashlib.sha256((ROOT / 'shadow-components/seplos_parser' / name).read_bytes()).hexdigest()
                                  for name in ['seplos_parser.cpp', 'seplos_parser.h', 'pic_status.h', 'supervised_trial.h']}
    filename = ('YAMBMS_OPERATING_LOGIC_RESULTS.json' if args.operating else
                'YAMBMS_RESTART_LOGIC_RESULTS.json' if args.restart_safe else
                'YAMBMS_HANDOVER_LOGIC_RESULTS.json' if args.handover else
                'YAMBMS_ADAPTER_RESULTS.json' if args.adapter else 'YAMBMS_LOGIC_RESULTS.json')
    if args.balancing_status:
        filename = 'YAMBMS_BALANCING_STATUS_LOGIC_RESULTS.json'
        report['status_profile'] = 'v3_sp18b_fw15_balancing'
    (args.output or ROOT / filename).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    raise SystemExit(any(item['exit_code'] != 0 for item in results))
