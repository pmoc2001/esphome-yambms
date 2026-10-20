"""Offline transaction-reader qualification. Never uploads or changes live settings."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from qualify_measurement_pair import ROOT, compile_test, digest, original
from qualify_top_charge import timer_ms


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--rejected-source', type=Path)
    p.add_argument('--compiled-root', type=Path)
    args = p.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(prefix='yam-transactions-') as temp:
        temp = Path(temp)
        for label, source in [('original', None)] + ([('rejected_candidate', args.rejected_source)] if args.rejected_source else []):
            root = temp / label
            shutil.copytree(ROOT / 'tests/stubs', root / 'tests/stubs')
            shutil.copyfile(ROOT / 'tests/transaction_reader_test.cpp', root / 'tests/transaction_reader_test.cpp')
            target = root / 'shadow-components/seplos_parser'
            target.mkdir(parents=True)
            for name in ('seplos_parser.cpp', 'seplos_parser.h', 'pic_status.h', 'supervised_trial.h'):
                (target / name).write_bytes((source / name).read_bytes() if source else original(name))
            binary = temp / (label + '-reproduction')
            subprocess.run(['g++', '-std=c++17', '-fsanitize=address,undefined', '-g',
                            '-DSHADOW_PARSER', '-DREPRODUCE_OLD_READER', '-I' + str(root / 'tests/stubs'),
                            str(root / 'tests/transaction_reader_test.cpp'), str(target / 'seplos_parser.cpp'),
                            '-o', str(binary)], check=True, timeout=90)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, result.stderr
            results.append({'test': label + '_fault_reproduction', 'passed': True, 'output': result.stdout})
            print(label, result.stdout, flush=True)
        for name in ('pic_status_test', 'status_profile_test', 'balancing_status_test', 'parser_test',
                     'shadow_parser_test', 'safety_preview_test', 'live_metadata_test', 'handover_policy_test',
                     'measurement_pair_test', 'transaction_reader_test'):
            binary = temp / name
            compile_test(name, binary)
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (name, result.stderr)
            results.append({'test': name, 'passed': True, 'output': result.stdout})
            print('PASS:', name, flush=True)
        for balancing in (False, True):
            report_path = temp / 'policy.json'
            command = [sys.executable, str(ROOT / 'tests/run_yambms_logic.py'), '--operating', '--output', str(report_path)]
            if balancing:
                command.append('--balancing-status')
            subprocess.run(command, check=True, timeout=180, stdout=subprocess.DEVNULL)
            policy = json.loads(report_path.read_text())
            assert len(policy['results']) == 37 and all(row['exit_code'] == 0 for row in policy['results'])
            results.append({'test': 'balancing_policy' if balancing else 'legacy_policy', 'passed': True, 'scenarios': 37})
            print('PASS:', results[-1]['test'], flush=True)

    component = ROOT / 'shadow-components/seplos_parser'
    hashes = {name: digest(component / name) for name in
              ('seplos_parser.cpp', 'seplos_parser.h', 'pic_status.h', 'supervised_trial.h', '__init__.py')}
    checks = {name + '_unchanged': (component / name).read_bytes() == original(name)
              for name in ('pic_status.h', 'supervised_trial.h')}
    checks['policy_unchanged'] = policy['source_sha256'] == json.loads((ROOT / 'qualification.json').read_text())['source_sha256']
    proof = {'scope': 'Offline reproduction and qualification; NOT live acceptance', 'live_installed': False,
             'real_traffic_replay_passed': False, 'pre_upload_gate_passed': False,
             'remaining_gate': 'Replay a full actual installation UART capture, including settings reads; then supervised live acceptance',
             'results': results, 'checks': checks, 'component_sha256': hashes}
    if args.compiled_root:
        # ESPHome resolves an explicit relative build_path beneath .esphome.
        build = args.compiled_root / '.esphome/.esphome/build/transaction-reader-v2'
        generated = (build / 'src/main.cpp').read_text()
        checks.update({
            'compiled_component_matches_tests': all(digest(build / 'src/esphome/components/seplos_parser' / n) == h
                                                    for n, h in hashes.items() if n.endswith(('.cpp', '.h'))),
            'transaction_tracking_enabled': 'seplos_v3->set_transaction_tracking(true)' in generated,
            'candidate_identity': 'transaction reader v3' in generated,
            'passive_only': 'seplos_uart->set_tx_pin' not in generated and 'output::TurnOnAction<>(rs485_transmit_enable)' not in generated,
            'caps_retained': all(s in generated for s in ('trial_cap(x, 55.2f)',
                'trial_cap(x, std::min(35.0f, seplos_v3->operating_limits().charge_limit))',
                'trial_cap(x, std::min(100.0f, seplos_v3->operating_limits().discharge_limit))')),
            'voltages_retained': 'yambms1_bulk_voltage->set_initial_value(55.2f)' in generated and
                                'yambms1_float_voltage->set_initial_value(53.6f)' in generated,
            'timers_retained': timer_ms(generated, 'yambms1_script_cutoff_timer') == 600000 and
                              timer_ms(generated, 'yambms1_script_eoc_timer') == 1800000,
            'persistence_retained': all(s in generated for s in (
                'live_bench_output_enabled->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF)',
                'yambms1_switch_charging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
                'yambms1_switch_discharging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
                'yambms1_switch_auto_cvl->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
                'yambms1_switch_auto_soc_limit->set_restore_mode(switch_::SWITCH_ALWAYS_OFF)')),
            'can_and_profile_retained': 'canbus_live_bench->set_bitrate(canbus::CAN_500KBPS)' in generated and
                'seplos_v3->set_status_profile("v3_sp18b_fw15_balancing")' in generated,
            'api_no_disconnect_reboot': 'api_apiserver_id->set_reboot_timeout(0)' in generated,
        })
        rollback = args.compiled_root / 'yambms-auto-cvl-persistent-20261005.ota.bin'
        checks['rollback_hash'] = digest(rollback) == 'd4d6f64b68e4ad67dffe18acf5b42e9b835d84037c493098feae9842ecdd550a'
        assert all(checks.values()), checks
        image = args.compiled_root / 'yambms-transaction-reader-v3-20261009.ota.bin'
        shutil.copyfile(build / 'build/firmware.ota.bin', image)
        image.chmod(0o600)
        proof.update(image=str(image), image_sha256=digest(image), rollback=str(rollback), rollback_sha256=digest(rollback))
    proof['passed'] = all(checks.values()) and all(r['passed'] for r in results)
    assert proof['passed'], checks
    args.output.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': True, 'live_installed': False, 'evidence': str(args.output)}))


if __name__ == '__main__':
    main()
