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
args = parser.parse_args()
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
        'seplos_parser_id': 'seplos_v3'}
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
#include <iostream>
#include <map>
#include <optional>
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
  float state = 0; bool present = true; unsigned selection = 0;
  bool has_state() const { return present; }
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
  std::function<void()> callback;
  bool is_running() const { return running; }
  void execute() { running = true; deadline = now_ms + delay_ms; }
  void stop() { running = false; }
  void poll() { if (running && now_ms >= deadline) { running = false; callback(); } }
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
functions = '\n'.join(f'{kind} run_{key}({"float x" if key.startswith("filter_") else ""}) {{\n{code[key]}\n}}'
                      for key, (kind, _) in bodies.items())
fixture = (ROOT / ('tests/yambms_adapter_scenarios.cpp' if args.adapter
                   else 'tests/yambms_logic_scenarios.cpp')).read_text()
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
    results = []
    for scenario in scenarios:
        result = subprocess.run([str(binary), scenario], capture_output=True, text=True)
        results.append({'scenario': scenario, 'exit_code': result.returncode,
                        'output': result.stdout.strip(), 'error': result.stderr.strip()})
    report = {'source_sha256': {key: hashlib.sha256(path.read_bytes()).hexdigest()
                               for key, path in paths.items()},
              'scope': 'Actual source lambdas; fake entities, timers and clock; no CAN transport',
              'results': results}
    if args.adapter:
        report['parser_sha256'] = {name: hashlib.sha256((ROOT / 'shadow-components/seplos_parser' / name).read_bytes()).hexdigest()
                                  for name in ['seplos_parser.cpp', 'seplos_parser.h', 'pic_status.h', 'supervised_trial.h']}
    filename = ('YAMBMS_OPERATING_LOGIC_RESULTS.json' if args.operating else
                'YAMBMS_RESTART_LOGIC_RESULTS.json' if args.restart_safe else
                'YAMBMS_HANDOVER_LOGIC_RESULTS.json' if args.handover else
                'YAMBMS_ADAPTER_RESULTS.json' if args.adapter else 'YAMBMS_LOGIC_RESULTS.json')
    (ROOT / filename).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    raise SystemExit(any(item['exit_code'] != 0 for item in results))
