"""Check a private compiled candidate and retain release/rollback evidence."""
import argparse
import json
from pathlib import Path
import re
import shutil

from qualify_measurement_pair import ROOT, digest

ROLLBACK_SHA = '65e88e795e313a872f69fa683912152f006b0d944f257a11d5fa31f307a7f06a'


def timer_ms(code, name):
    block = re.search(r'Automation<int>\(' + re.escape(name) +
                      r'\);(.*?)script::ScriptExecuteAction', code, re.S)
    assert block, name
    delay = re.search(r'->set_delay\(\[\]\(int attempt\) -> uint32_t \{\s*return (\d+);', block[1])
    assert delay, name
    return int(delay[1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-root', type=Path, required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    parser.add_argument('--rollback', type=Path, required=True)
    args = parser.parse_args()
    proof = json.loads((args.evidence_dir / 'qualification.json').read_text())
    assert proof['passed'] and not proof['live_installed']
    build = args.build_root / '.esphome/build/finish-session-fix'
    generated = (build / 'src/main.cpp').read_text()
    checks = {
        'tested_core_compiled': proof['core_sha256'] == digest(args.build_root / 'yam-packages/yambms/yambms_core.yaml') ==
            digest(ROOT / 'package-overrides/yambms/yambms_core.yaml'),
        'tested_wrapper_compiled': proof['wrapper_sha256'] == digest(args.build_root / 'yambms-finish-session-fix.yaml'),
        'installed_reader_preserved_in_build': all(
            digest(build / 'src/esphome/components/seplos_parser' / name) == expected
            for name, expected in proof['parser_sha256'].items()),
        'transaction_reader_enabled': 'seplos_v3->set_transaction_tracking(true)' in generated,
        'candidate_identity': 'finish session fix v1' in generated,
        'timer_durations_preserved': timer_ms(generated, 'yambms1_script_cutoff_timer') == 600000 and
            timer_ms(generated, 'yambms1_script_eoc_timer') == 1800000,
        'attempt_parameters_generated': 'Automation<int, bool>(yambms1_script_eoc)' in generated and
            'attempt != yambms1_var_finish_attempt->value()' in generated,
        'raw_guard_and_balancer_check_generated': all(value in generated for value in (
            'bms1_current->get_raw_state()', 'bms1_max_cell_voltage->get_raw_state()',
            'allow_balancing || !seplos_v3->adapter_equalizing()',
            'yambms1_var_finish_source_guard->value()(backstop)')),
        'limits_unchanged': all(value in generated for value in (
            'trial_cap(x, 55.2f)',
            'trial_cap(x, std::min(35.0f, seplos_v3->operating_limits().charge_limit))',
            'trial_cap(x, std::min(100.0f, seplos_v3->operating_limits().discharge_limit))',
            'yambms1_bulk_voltage->set_initial_value(55.2f)',
            'yambms1_float_voltage->set_initial_value(53.6f)')),
        'saved_controls_unchanged': all(value in generated for value in (
            'live_bench_output_enabled->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF)',
            'yambms1_switch_charging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
            'yambms1_switch_discharging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
            'yambms1_switch_auto_cvl->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
            'yambms1_switch_auto_soc_limit->set_restore_mode(switch_::SWITCH_ALWAYS_OFF)')),
        'can_and_profile_unchanged': 'canbus_live_bench->set_bitrate(canbus::CAN_500KBPS)' in generated and
            'seplos_v3->set_status_profile("v3_sp18b_fw15_balancing")' in generated,
        'read_only_single_pack': 'seplos_uart->set_tx_pin' not in generated and
            'output::TurnOnAction<>(rs485_transmit_enable)' not in generated and 'bms2_' not in generated,
        'api_encrypted_no_disconnect_reboot': 'api_apiserver_id->set_noise_psk(' in generated and
            'api_apiserver_id->set_reboot_timeout(0)' in generated,
        'rollback_matches_installed_release': digest(args.rollback) == ROLLBACK_SHA,
    }
    assert all(checks.values()), {key: value for key, value in checks.items() if not value}
    image = args.evidence_dir / 'yambms-finish-session-v1-20261015.ota.bin'
    rollback = args.evidence_dir / 'rollback-transaction-reader-v3-20261009.ota.bin'
    for source, destination in ((build / 'build/firmware.ota.bin', image), (args.rollback, rollback)):
        shutil.copyfile(source, destination)
        destination.chmod(0o600)
    report = {'scope': 'Offline candidate build; live installation and charge acceptance pending',
              'passed': True, 'live_installed': False, 'checks': checks,
              'image': str(image), 'image_sha256': digest(image),
              'rollback': str(rollback), 'rollback_sha256': digest(rollback),
              'generated_cpp_sha256': digest(build / 'src/main.cpp'),
              'qualification_sha256': digest(args.evidence_dir / 'qualification.json')}
    destination = args.evidence_dir / 'build-verification.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': True, 'checks': len(checks), 'live_installed': False,
                      'evidence': str(destination), 'image_sha256': report['image_sha256']}))


if __name__ == '__main__':
    main()
