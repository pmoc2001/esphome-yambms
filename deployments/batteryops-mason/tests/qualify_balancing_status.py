"""Offline regression and compiled-image checks. Never uploads or changes GX."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiled', action='store_true')
    args = parser.parse_args()
    results = []
    with tempfile.TemporaryDirectory(prefix='yam-balancing-status-') as temporary:
        for name in ('pic_status_test', 'status_profile_test', 'balancing_status_test',
                     'parser_test', 'shadow_parser_test', 'safety_preview_test',
                     'live_metadata_test', 'handover_policy_test'):
            binary = Path(temporary) / name
            subprocess.run(['g++', '-std=c++17', '-fsanitize=address,undefined', '-g', '-DSHADOW_PARSER',
                            '-I' + str(ROOT/'tests/stubs'),
                            str(ROOT/'tests'/f'{name}.cpp'),
                            str(ROOT/'shadow-components/seplos_parser/seplos_parser.cpp'),
                            '-o', str(binary)], check=True, timeout=60)
            value = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
            results.append({'test': name, 'exit_code': value.returncode,
                            'output': value.stdout.strip(), 'error': value.stderr.strip()})
            print(name, value.returncode, value.stdout.strip(), flush=True)
            if value.returncode:
                raise RuntimeError(value.stderr)
    for balancing in (False, True):
        destination = ROOT / ('BALANCING_STATUS_OPERATING.json' if balancing else
                              'BALANCING_STATUS_LEGACY_REGRESSION.json')
        command = [sys.executable, str(ROOT/'tests/run_yambms_logic.py'), '--operating',
                   '--output', str(destination)]
        if balancing:
            command.append('--balancing-status')
        value = subprocess.run(command, capture_output=True, text=True, timeout=120)
        if value.returncode:
            raise RuntimeError(value.stderr + value.stdout)
        report = json.loads(destination.read_text())
        assert len(report['results']) == 37 and all(r['exit_code'] == 0 for r in report['results'])
        results.append({'test': 'operating_balancing' if balancing else 'operating_legacy',
                        'exit_code': 0, 'scenario_count': 37, 'evidence': str(destination)})
    tests = json.loads((ROOT/'BALANCING_STATUS_OPERATING.json').read_text())
    prior = json.loads((ROOT/'qualification.json').read_text())
    checks = {'all_native_suites_pass': all(r['exit_code'] == 0 for r in results),
              'all_operating_policy_sources_unchanged': tests['source_sha256'] == prior['source_sha256'],
              'frame_decoder_unchanged': tests['parser_sha256']['seplos_parser.cpp'] ==
                                         prior['parser_sha256']['seplos_parser.cpp'],
              'cell_temperature_guard_unchanged': tests['parser_sha256']['supervised_trial.h'] ==
                                                 prior['parser_sha256']['supervised_trial.h']}
    image = None
    if args.compiled:
        build = ROOT / '.esphome/build/batteryops-waveshare'
        code = (build/'src/main.cpp').read_text()
        checks.update({
            'explicit_new_profile': 'seplos_v3->set_status_profile("v3_sp18b_fw15_balancing")' in code,
            'release_role': '35A charge / 100A discharge - balance status qualified' in code,
            'compiled_parser_matches_tested_source': all(
                digest(build/'src/esphome/components/seplos_parser'/name) == digest(
                    ROOT/'shadow-components/seplos_parser'/name) == expected
                for name, expected in tests['parser_sha256'].items()),
            'single_real_passive_reader': 'bms2_' not in code and 'replay_parser' not in code and
                'seplos_uart->set_tx_pin' not in code and
                'output::TurnOnAction<>(rs485_transmit_enable)' not in code,
            'can_500k': 'canbus_live_bench->set_bitrate(canbus::CAN_500KBPS)' in code,
            'wire_caps_retained': all(part in code for part in (
                'trial_cap(x, 55.2f)',
                'trial_cap(x, std::min(35.0f, seplos_v3->operating_limits().charge_limit))',
                'trial_cap(x, std::min(100.0f, seplos_v3->operating_limits().discharge_limit))')),
            'encrypted_api_retained': 'api_apiserver_id->set_noise_psk(' in code,
            'ha_loss_does_not_reboot': 'api_apiserver_id->set_reboot_timeout(0)' in code,
            'master_stop_persistence_retained':
                'live_bench_output_enabled->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF)' in code,
            'directional_stop_persistence_retained': all(
                'yambms1_switch_'+name+'->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)' in code
                for name in ('charging', 'discharging')),
            'three_intent_commits': sum(line.strip() ==
                'const bool committed = esphome::global_preferences->sync();'
                for line in code.splitlines()) == 3,
        })
        image = ROOT/'yambms-balancing-status-fix-20261005.ota.bin'
        rollback = ROOT/'yambms-ha-20261004T154845Z.ota.bin'
        old = json.loads((ROOT/'HA_BUILD_20261004T154845Z.json').read_text())
        checks['rollback_matches_installed_release'] = digest(rollback) == old['image_sha256']
        assert all(checks.values()), checks
        shutil.copyfile(build/'build/firmware.ota.bin', image)
    assert all(checks.values()), checks
    proof = {'passed': True, 'scope': 'Offline only; no OTA or live GX proof',
             'checks': checks, 'results': results, 'parser_sha256': tests['parser_sha256'],
             'live_installed': False, 'image': str(image) if image else None}
    if image:
        proof.update(image_sha256=digest(image), rollback=str(rollback),
                     rollback_sha256=digest(rollback), generated_cpp_sha256=digest(build/'src/main.cpp'))
    destination = ROOT/'BALANCING_STATUS_QUALIFICATION_20261005.json'
    destination.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': True, 'checks': len(checks), 'evidence': str(destination),
                      'live_installed': False}), flush=True)


if __name__ == '__main__':
    main()
