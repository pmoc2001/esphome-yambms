"""Offline finishing repair acceptance. No device access or uploads."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from qualify_measurement_pair import ROOT, compile_test, digest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    results = []
    for name, flags in [
        ('core', []), ('operating', ['--operating']),
        ('operating-balancing', ['--operating', '--balancing-status']),
        ('finish-core', ['--finish-session']),
        ('finish-parser', ['--operating', '--finish-session']),
    ]:
        destination = args.output_dir / (name + '.json')
        run = subprocess.run([sys.executable, str(ROOT / 'tests/run_yambms_logic.py'),
                              *flags, '--output', str(destination)],
                             text=True, capture_output=True, timeout=180)
        if run.returncode:
            raise RuntimeError(f'{name} failed; inspect {destination}\n{run.stderr}')
        report = json.loads(destination.read_text())
        assert all(row['exit_code'] == 0 for row in report['results'])
        results.append({'suite': name, 'passed': True, 'scenarios': len(report['results'])})
        print(f'PASS {name}: {len(report["results"])} scenarios', flush=True)
    with tempfile.TemporaryDirectory(prefix='yam-finish-native-') as temp:
        for name in ('pic_status_test', 'status_profile_test', 'balancing_status_test',
                     'parser_test', 'shadow_parser_test', 'safety_preview_test',
                     'live_metadata_test', 'handover_policy_test',
                     'measurement_pair_test', 'transaction_reader_test'):
            binary = Path(temp) / name
            compile_test(name, binary)
            run = subprocess.run([str(binary)], text=True, capture_output=True, timeout=25)
            assert run.returncode == 0, (name, run.stderr)
            results.append({'suite': name, 'passed': True, 'output': run.stdout.strip()})
            print('PASS ' + name, flush=True)
    # Compare with the installed reader release, not Git HEAD's older parser.
    installation = ROOT / 'tests/installed-reader-baseline.json'
    installed = json.loads(installation.read_text())
    prior_policy = json.loads((ROOT / 'qualification.json').read_text())['source_sha256']
    operating = json.loads((args.output_dir / 'operating.json').read_text())
    checks = {
        'reader_and_safety_sources_match_installed_release': all(
            digest(ROOT / 'shadow-components/seplos_parser' / name) == expected
            for name, expected in installed['component_sha256'].items()),
        'non_finishing_policy_unchanged': all(
            operating['source_sha256'][name] == expected
            for name, expected in prior_policy.items() if name != 'core'),
        'materialized_core_matches_override':
            digest(ROOT / 'yam-packages/yambms/yambms_core.yaml') ==
            digest(ROOT / 'package-overrides/yambms/yambms_core.yaml'),
    }
    assert all(checks.values()), checks
    proof = {'scope': 'Offline source-lambda and real-parser tests; not live charge proof',
             'live_installed': False, 'passed': True, 'checks': checks, 'results': results,
             'core_sha256': operating['source_sha256']['core'],
             'parser_sha256': operating['parser_sha256'],
             'wrapper_sha256': digest(ROOT / 'yambms-finish-session-fix.yaml')}
    destination = args.output_dir / 'qualification.json'
    destination.write_text(json.dumps(proof, indent=2) + '\n')
    print(f'Offline qualification passed: {destination}', flush=True)


if __name__ == '__main__':
    main()
