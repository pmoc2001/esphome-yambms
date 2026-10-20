"""Check the timer-only trial against qualified policy and compiled output.

Offline only. The generated image contains private credentials and stays local.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

import yaml

ROOT = Path(__file__).resolve().parents[1]
ROLE = ('OPERATING YAMBMS - Seplos SP18B FW1.5 - 35A charge / 100A discharge'
        ' - balance status qualified - cut-off 600s trial')
ROLLBACK_SHA = '1311515ddad4780d388004b901f7072f1c3119e9d93eb77197c5d3679a0ac757'


class Loader(yaml.SafeLoader):
    pass


Loader.add_multi_constructor('!', lambda loader, tag, node:
    loader.construct_scalar(node) if isinstance(node, yaml.ScalarNode) else
    loader.construct_sequence(node) if isinstance(node, yaml.SequenceNode) else
    loader.construct_mapping(node))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def timer_ms(code, name):
    block = re.search(r'new\(\w+\) Automation<>\(' + re.escape(name) +
                      r'\);(.*?)new\(\w+\) script::ScriptExecuteAction', code, re.S)
    assert block, name
    value = re.search(r'->set_delay\(\[\]\(\) -> uint32_t \{\s*return (\d+);',
                      block.group(1))
    assert value, name
    return int(value.group(1))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--auto-cvl-persistence', action='store_true')
    args = parser.parse_args()
    wrapper = ROOT/('yambms-auto-cvl-persistent.yaml' if args.auto_cvl_persistence
                    else 'yambms-top-charge-trial.yaml')
    config = yaml.load((ROOT/'yambms-top-charge-trial.yaml').read_text(), Loader=Loader)
    prior = json.loads((ROOT/'qualification.json').read_text())
    regression = json.loads((ROOT/'BALANCING_STATUS_OPERATING.json').read_text())
    build = ROOT/'.esphome/build/batteryops-waveshare'
    code = (build/'src/main.cpp').read_text()
    rollback = ROOT/('yambms-top-charge-trial-20261005.ota.bin' if args.auto_cvl_persistence
                     else 'yambms-balancing-status-fix-20261005.ota.bin')
    rollback_sha = ('8d49f34160f75f0889f6bf45e82abee018f37977d9b55c7d67ca28d03632184a'
                    if args.auto_cvl_persistence else ROLLBACK_SHA)
    role = ROLE + (' - persistent Auto CVL' if args.auto_cvl_persistence else '')
    checks = {
        'only_timer_substitution': config['substitutions'] == {'yambms_cutoff_timer': '600'},
        'current_qualified_release_inherited': config['packages'] ==
            {'qualified_operating': 'yambms-balancing-status-fix.yaml'},
        'all_37_operating_scenarios_pass': len(regression['results']) == 37 and
            all(row['exit_code'] == 0 for row in regression['results']),
        'policy_sources_unchanged': regression['source_sha256'] == prior['source_sha256'],
        'cutoff_delay_600_seconds': timer_ms(code, 'yambms1_script_cutoff_timer') == 600000,
        'maximum_eoc_delay_30_minutes': timer_ms(code, 'yambms1_script_eoc_timer') == 1800000,
        'compiled_parser_and_guards_match_tested_sources': all(
            digest(build/'src/esphome/components/seplos_parser'/name) ==
            digest(ROOT/'shadow-components/seplos_parser'/name) == expected
            for name, expected in regression['parser_sha256'].items()),
        'balance_status_profile_retained':
            'seplos_v3->set_status_profile("v3_sp18b_fw15_balancing")' in code,
        'voltage_and_current_caps_retained': all(value in code for value in (
            'trial_cap(x, 55.2f)',
            'trial_cap(x, std::min(35.0f, seplos_v3->operating_limits().charge_limit))',
            'trial_cap(x, std::min(100.0f, seplos_v3->operating_limits().discharge_limit))',
            'yambms1_bulk_voltage->set_initial_value(55.2f)',
            'yambms1_float_voltage->set_initial_value(53.6f)')),
        '80_percent_limit_stays_disabled':
            'yambms1_switch_auto_soc_limit->set_restore_mode(switch_::SWITCH_ALWAYS_OFF)' in code,
        'single_passive_reader': 'bms2_' not in code and 'replay_parser' not in code and
            'seplos_uart->set_tx_pin' not in code and
            'output::TurnOnAction<>(rs485_transmit_enable)' not in code,
        'can_500k_retained': 'canbus_live_bench->set_bitrate(canbus::CAN_500KBPS)' in code,
        'encrypted_api_retained': 'api_apiserver_id->set_noise_psk(' in code,
        'ha_loss_does_not_reboot': 'api_apiserver_id->set_reboot_timeout(0)' in code,
        'saved_stops_retained': all(value in code for value in (
            'live_bench_output_enabled->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF)',
            'yambms1_switch_charging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)',
            'yambms1_switch_discharging->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)')),
        'three_intent_commits_retained': sum(line.strip() ==
            'const bool committed = esphome::global_preferences->sync();'
            for line in code.splitlines()) == 3,
        'trial_identity_and_visible_timer': role in code and
            'App.register_sensor(qualification_cutoff_seconds, "Cut-Off Confirmation Time"' in code,
        'rollback_matches_running_release': digest(rollback) == rollback_sha,
    }
    if args.auto_cvl_persistence:
        persistence = yaml.load(wrapper.read_text(), Loader=Loader)
        checks.update({
            'persistence_inherits_current_trial': persistence['packages'] ==
                {'qualified_operating': 'yambms-top-charge-trial.yaml'},
            'only_auto_cvl_switch_overridden': len(persistence['switch']) == 1 and
                persistence['switch'][0]['id'] == 'yambms1_switch_auto_cvl',
            'native_saved_auto_cvl_default_on':
                'yambms1_switch_auto_cvl->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_ON)' in code,
            'auto_cvl_state_immediately_synced':
                'Could not save Auto CVL setting' in code and
                'if (!esphome::global_preferences->sync())' in code,
            'voltage_boost_still_zero':
                'yambms1_auto_cvl_boost_charge_v->set_initial_value(0.0f)' in code,
        })
    assert all(checks.values()), {k: v for k, v in checks.items() if not v}
    image = ROOT/('yambms-auto-cvl-persistent-20261005.ota.bin' if args.auto_cvl_persistence
                  else 'yambms-top-charge-trial-20261005.ota.bin')
    shutil.copyfile(build/'build/firmware.ota.bin', image)
    proof = {'passed': True, 'live_installed': False, 'checks': checks,
             'scope': ('Authorized Auto CVL persistence only; no charging policy or BMS/SmartShunt writes'
                       if args.auto_cvl_persistence else
                       'Authorized 600-second cut-off trial; no BMS or SmartShunt writes'),
             'role': role, 'image': str(image), 'image_sha256': digest(image),
             'rollback': str(rollback), 'rollback_sha256': digest(rollback),
             'generated_cpp_sha256': digest(build/'src/main.cpp'),
             'wrapper_sha256': digest(wrapper),
             'source_sha256': regression['source_sha256'],
             'parser_sha256': regression['parser_sha256']}
    path = ROOT/('AUTO_CVL_PERSISTENCE_QUALIFICATION_20261005.json' if args.auto_cvl_persistence
                 else 'TOP_CHARGE_QUALIFICATION_20261005.json')
    path.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': True, 'checks': len(checks), 'evidence': str(path)}))


if __name__ == '__main__':
    main()
