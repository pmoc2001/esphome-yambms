"""Reproduce the old race, qualify its repair, and optionally package a build.

Local files and native tests only. No uploads, remote writes, or GX changes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from qualify_top_charge import timer_ms

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
PREFIX = 'deployments/batteryops-mason/shadow-components/seplos_parser/'
ROLE = ('OPERATING YAMBMS - Seplos SP18B FW1.5 - 35A charge / 100A discharge'
        ' - balance status qualified - cut-off 600s trial - persistent Auto CVL'
        ' - paired measurements v1')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def original(name):
    return subprocess.check_output(['git', '-C', str(REPO), 'show', 'HEAD:' + PREFIX + name], timeout=10)


def compile_test(name, destination, root=ROOT):
    subprocess.run(['g++', '-std=c++17', '-fsanitize=address,undefined', '-g', '-DSHADOW_PARSER',
                    '-I' + str(root/'tests/stubs'), str(root/'tests'/f'{name}.cpp'),
                    str(root/'shadow-components/seplos_parser/seplos_parser.cpp'),
                    '-o', str(destination)], check=True, timeout=90)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiled', action='store_true')
    parser.add_argument('--build-root', type=Path, default=ROOT)
    args = parser.parse_args()
    results = []
    with tempfile.TemporaryDirectory(prefix='yam-measurement-pair-') as temporary:
        temporary = Path(temporary)
        baseline = temporary/'baseline'
        shutil.copytree(ROOT/'tests/stubs', baseline/'tests/stubs')
        shutil.copyfile(ROOT/'tests/measurement_pair_test.cpp', baseline/'tests/measurement_pair_test.cpp')
        directory = baseline/'shadow-components/seplos_parser'
        directory.mkdir(parents=True)
        for name in ('seplos_parser.cpp', 'seplos_parser.h', 'pic_status.h', 'supervised_trial.h'):
            (directory/name).write_bytes(original(name))
        binary = temporary/'old-race'
        compile_test('measurement_pair_test', binary, baseline)
        value = subprocess.run([str(binary)], capture_output=True, text=True, timeout=20)
        assert value.returncode != 0 and 'Assertion' in value.stderr and 'metadata_ready' in value.stderr, value
        results.append({'test': 'old_reader_reproduces_load_step_race', 'passed': True,
                        'expected_failure': value.stderr.strip()})
        print('PASS: original reader fails the new load-step regression', flush=True)
        for name in ('pic_status_test', 'status_profile_test', 'balancing_status_test',
                     'parser_test', 'shadow_parser_test', 'safety_preview_test',
                     'live_metadata_test', 'handover_policy_test', 'measurement_pair_test'):
            binary = temporary/name
            compile_test(name, binary)
            value = subprocess.run([str(binary)], capture_output=True, text=True, timeout=20)
            results.append({'test': name, 'passed': value.returncode == 0,
                            'output': value.stdout.strip(), 'error': value.stderr.strip()})
            print(name, value.returncode, value.stdout.strip(), flush=True)
            assert value.returncode == 0, value.stderr
    for balancing in (False, True):
        destination = args.build_root/('MEASUREMENT_PAIR_OPERATING.json' if balancing else
                                       'MEASUREMENT_PAIR_LEGACY.json')
        command = [sys.executable, str(ROOT/'tests/run_yambms_logic.py'), '--operating',
                   '--output', str(destination)]
        if balancing:
            command.append('--balancing-status')
        subprocess.run(command, check=True, timeout=180, stdout=subprocess.DEVNULL)
        report = json.loads(destination.read_text())
        assert len(report['results']) == 37 and all(row['exit_code'] == 0 for row in report['results'])
        results.append({'test': 'operating_balancing' if balancing else 'operating_legacy',
                        'passed': True, 'scenario_count': 37})
        print('PASS:', results[-1]['test'], '37 scenarios', flush=True)
    prior = json.loads((ROOT/'qualification.json').read_text())
    code = (ROOT/'shadow-components/seplos_parser/seplos_parser.cpp').read_text()
    old = original('seplos_parser.cpp').decode()
    checks = {
        'all_native_and_policy_tests_pass': all(row['passed'] for row in results),
        'upstream_operating_policy_unchanged': report['source_sha256'] == prior['source_sha256'],
        'documented_status_mapping_unchanged': original('pic_status.h') ==
            (ROOT/'shadow-components/seplos_parser/pic_status.h').read_bytes(),
        'cell_temperature_guards_unchanged': original('supervised_trial.h') ==
            (ROOT/'shadow-components/seplos_parser/supervised_trial.h').read_bytes(),
        'header_lengths_and_crc_validation_unchanged':
            code.split('bool SeplosParser::is_valid_header()')[1].split('std::string join_list')[0] ==
            old.split('bool SeplosParser::is_valid_header()')[1].split('std::string join_list')[0],
        'crc_algorithm_unchanged': code.split('const uint16_t crc_table')[1].split('void SeplosParser::dump_config')[0] ==
            old.split('const uint16_t crc_table')[1].split('void SeplosParser::dump_config')[0],
    }
    proof = {'scope': 'Offline qualification; existing-device checks are separate; no OTA',
             'live_installed': False, 'results': results, 'checks': checks,
             'parser_sha256': report['parser_sha256'], 'policy_sha256': report['source_sha256']}
    if args.compiled:
        build = args.build_root/'.esphome/build/batteryops-waveshare'
        generated = (build/'src/main.cpp').read_text()
        checks.update({
            'compiled_parser_matches_tested_source': all(
                digest(build/'src/esphome/components/seplos_parser'/name) == expected
                for name, expected in report['parser_sha256'].items()),
            'release_identity_and_admission_diagnostic': ROLE in generated and
                'seplos_v3->diagnostic_metadata()' in generated,
            'balance_status_profile_retained': 'seplos_v3->set_status_profile("v3_sp18b_fw15_balancing")' in generated,
            'wire_caps_unchanged': all(text in generated for text in (
                'trial_cap(x, 55.2f)',
                'trial_cap(x, std::min(35.0f, seplos_v3->operating_limits().charge_limit))',
                'trial_cap(x, std::min(100.0f, seplos_v3->operating_limits().discharge_limit))')),
            'bulk_and_float_unchanged': 'yambms1_bulk_voltage->set_initial_value(55.2f)' in generated and
                'yambms1_float_voltage->set_initial_value(53.6f)' in generated,
            'timers_unchanged': timer_ms(generated, 'yambms1_script_cutoff_timer') == 600000 and
                timer_ms(generated, 'yambms1_script_eoc_timer') == 1800000,
            'saved_stop_intents_retained': all(text in generated for text in (
                'live_bench_output_enabled->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF)',
                'yambms1_switch_charging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
                'yambms1_switch_discharging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)')),
            'auto_cvl_persistence_retained':
                'yambms1_switch_auto_cvl->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)' in generated,
            '80_percent_limit_stays_off':
                'yambms1_switch_auto_soc_limit->set_restore_mode(switch_::SWITCH_ALWAYS_OFF)' in generated,
            'single_passive_reader': 'bms2_' not in generated and 'replay_parser' not in generated and
                'seplos_uart->set_tx_pin' not in generated and
                'output::TurnOnAction<>(rs485_transmit_enable)' not in generated,
            'can_500k_retained': 'canbus_live_bench->set_bitrate(canbus::CAN_500KBPS)' in generated,
            'encrypted_api_and_no_ha_disconnect_reboot': 'api_apiserver_id->set_noise_psk(' in generated and
                'api_apiserver_id->set_reboot_timeout(0)' in generated,
        })
        rollback = args.build_root/'yambms-auto-cvl-persistent-20261005.ota.bin'
        release = json.loads((args.build_root/'AUTO_CVL_PERSISTENCE_QUALIFICATION_20261005.json').read_text())
        checks['rollback_image_hash_verified'] = digest(rollback) == release['image_sha256']
        assert all(checks.values()), {key: value for key, value in checks.items() if not value}
        image = args.build_root/'yambms-measurement-pair-fix-20261006.ota.bin'
        shutil.copyfile(build/'build/firmware.ota.bin', image)
        proof.update(image=str(image), image_sha256=digest(image), rollback=str(rollback),
                     rollback_sha256=digest(rollback), generated_cpp_sha256=digest(build/'src/main.cpp'))
    assert all(checks.values()), checks
    proof['passed'] = True
    destination = args.build_root/'MEASUREMENT_PAIR_QUALIFICATION_20261006.json'
    destination.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': True, 'checks': len(checks), 'evidence': str(destination),
                      'live_installed': False}), flush=True)


if __name__ == '__main__':
    main()
