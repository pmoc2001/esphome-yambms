"""Read the installed system and replay decoded samples locally, without OTA.

HTTP observations are sequential, NOT atomic serial captures. Reconstructed
frames qualify input decoding only, not timing or physical candidate control.
"""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import tempfile
import time
from urllib.parse import quote
from urllib.request import ProxyHandler, build_opener

from qualify_measurement_pair import compile_test

HTTP = build_opener(ProxyHandler({}))


def read(url):
    with HTTP.open(url, timeout=8) as response:
        return json.load(response)


def device(path):
    return read('http://192.168.1.221/' + quote(path, safe='/'))['value']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    names = ['pack_voltage', 'current', 'remaining_capacity', 'total_capacity',
             'soc', 'soh', 'cycle_count', 'min_cell_voltage', 'max_cell_voltage',
             'min_cell_temp', 'max_cell_temp'] + [f'cell_{i}' for i in range(1, 17)] + [
             'cell_temp_1', 'cell_temp_2', 'cell_temp_3', 'cell_temp_4', 'case_temp',
             'power_temp', 'maxchgcurt', 'maxdiscurt']
    samples = []
    with tempfile.TemporaryDirectory(prefix='yam-live-readonly-') as temporary:
        binary = Path(temporary)/'measurement-replay'
        compile_test('measurement_pair_test', binary)
        for index in range(4):
            started = datetime.now(timezone.utc).isoformat()
            raw = {name: device('sensor/Seplos Shadow/bms0 ' + name) for name in names}
            pic = device('text_sensor/Qualification/Raw PIC Hex')
            value = subprocess.run([str(binary), '--live-sample'],
                                   input=' '.join(str(raw[name]) for name in names) + ' ' + pic,
                                   capture_output=True, text=True, timeout=20)
            latest = read('http://192.168.1.150:8780/api/latest')
            state = latest['state']
            sample = {
                'started_at': started, 'completed_at': datetime.now(timezone.utc).isoformat(),
                'raw_decoded_values': raw, 'raw_pic_hex': pic,
                'candidate_local_replay': {'exit_code': value.returncode, 'output': value.stdout.strip(),
                                           'error': value.stderr.strip()},
                'installed_role': device('text_sensor/Qualification/Firmware Role'),
                'installed_uptime_s': device('sensor/Uptime'),
                'installed_fresh': device('binary_sensor/Qualification/Complete Data Fresh'),
                'installed_metadata_ready': device('binary_sensor/Qualification/Metadata Ready'),
                'installed_diagnostics': device('text_sensor/Qualification/Documented Status Diagnostics'),
                'installed_can': device('text_sensor/Qualification/CAN Controller Status'),
                'gx_timestamp': state['timestamp'], 'gx_data_stale': latest['data_stale'],
                'active_alerts': latest['active_alerts'], 'soc_percent': state['soc_percent'],
                'gx_ccl_a': state['max_charge_current_a'], 'gx_dcl_a': state['max_discharge_current_a'],
                'multiplus': state['multiplus'],
            }
            samples.append(sample)
            args.output.write_text(json.dumps({'scope': 'Read-only installed-system preflight and local replay of decoded web samples; NOT candidate live-control proof',
                                              'remote_writes': False, 'uploaded': False, 'samples': samples}, indent=2) + '\n')
            assert value.returncode == 0, value.stderr
            assert sample['installed_fresh'] and sample['installed_metadata_ready']
            assert not sample['gx_data_stale'] and not sample['active_alerts']
            assert sample['multiplus']['active_alarm_count'] == 0
            assert 210 <= sample['multiplus']['input_voltage_v'] <= 255
            assert 'paired measurements v1' not in sample['installed_role']
            print(json.dumps({'sample': index + 1, 'replay_pass': True, 'soc': sample['soc_percent'],
                              'ccl': sample['gx_ccl_a'], 'dcl': sample['gx_dcl_a'],
                              'alarms': sample['multiplus']['active_alarm_count'], 'uploaded': False}), flush=True)
            if index < 3:
                time.sleep(10)
        assert samples[-1]['installed_uptime_s'] >= samples[0]['installed_uptime_s']
    print('PASS: four read-only live samples; existing firmware unchanged; candidate replay passed', flush=True)


if __name__ == '__main__':
    main()
