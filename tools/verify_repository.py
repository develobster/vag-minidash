"""Repeatable offline acceptance; never opens CAN, a VCI or a serial device."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def run(command, output, name):
    started = time.monotonic()
    try:
        with (output / (name + '.log')).open('xb') as stream:
            result = subprocess.run(command, cwd=ROOT, stdout=stream,
                                    stderr=subprocess.STDOUT, check=False)
        code = result.returncode
        error = None
    except OSError as exc:
        code, error = None, str(exc)
    row = dict(command=[str(value) for value in command], exit_code=code,
               seconds=round(time.monotonic() - started, 3),
               status='PASS' if code == 0 else 'FAIL')
    if error:
        row['error'] = error
    print(name + ': ' + row['status'], flush=True)
    return row


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True,
                        help='New directory for logs/summary; existing paths rejected')
    parser.add_argument('--native-build-dir', type=Path,
                        help='Previously configured CMake directory with cJSON tests')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--ctest', default='ctest')
    parser.add_argument('--node', default='node')
    args = parser.parse_args(argv)
    args.output_dir = args.output_dir.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=False)
    gates = {}
    missing = [name for name in ('can', 'isotp', 'cantools', 'udsoncan')
               if importlib.util.find_spec(name) is None]
    gates['dependencies'] = dict(status='FAIL' if missing else 'PASS', missing=missing)
    gates['python'] = run([sys.executable, '-m', 'unittest', 'discover',
                           '-s', 'tests', '-v'], args.output_dir, 'python')
    if args.native_build_dir is None:
        for name in ('native_build', 'ctest', 'web', 'standalone_virtualbus', 'analytics_replay'):
            gates[name] = dict(status='NOT_RUN', reason='No --native-build-dir')
    else:
        build = args.native_build_dir.resolve()
        gates['native_build'] = run([args.cmake, '--build', str(build)],
                                    args.output_dir, 'native_build')
        if gates['native_build']['status'] != 'PASS':
            for name in ('ctest', 'web', 'standalone_virtualbus', 'analytics_replay'):
                gates[name] = dict(status='NOT_RUN', reason='Fresh native build failed')
        else:
            try:
                inventory = subprocess.run([args.ctest, '--test-dir', str(build),
                                            '--show-only=json-v1'], cwd=ROOT,
                                           capture_output=True, check=False)
                names = {item['name'] for item in json.loads(inventory.stdout)['tests']}
                listed = inventory.returncode == 0
            except (OSError, ValueError, KeyError, TypeError):
                names = set()
                listed = False
            gates['native_test_inventory'] = dict(
                status='PASS' if listed and
                {'portable-core', 'configuration-json', 'standalone-acquisition', 'derived-analytics', 'instrument-profile-contract', 'backlight-driver-contract'} <= names else 'FAIL',
                tests=sorted(names))
            gates['ctest'] = run([args.ctest, '--test-dir', str(build),
                                  '--output-on-failure', '--no-tests=error'],
                                 args.output_dir, 'ctest')
            harness = build / ('minidash-acquisition-harness.exe' if sys.platform == 'win32'
                               else 'minidash-acquisition-harness')
            gates['standalone_virtualbus'] = run(
                [sys.executable, str(ROOT / 'tools/verify_standalone.py'), '--harness', str(harness)],
                args.output_dir, 'standalone_virtualbus')
            gates['analytics_replay'] = run(
                [sys.executable, str(ROOT/'tools/verify_analytics.py'), '--harness', str(build/'minidash-analytics-harness'),
                 '--output', str(args.output_dir/'analytics-replay.json')], args.output_dir, 'analytics_replay')
            # Generate editor registries from the current firmware C sources,
            # rather than accepting an old manually copied browser fixture.
            candidates = [build / 'minidash-json-tests',
                          build / 'minidash-json-tests.exe']
            binary = next((path for path in candidates if path.is_file()), None)
            if binary is None:
                gates['web'] = dict(status='FAIL', reason='cJSON test executable missing')
            else:
                fixture = args.output_dir / 'web-fixture.json'
                try:
                    with fixture.open('xb') as stream:
                        generated = subprocess.run([str(binary), '--fixture'], cwd=ROOT,
                                                   stdout=stream, check=False)
                    generated_ok = generated.returncode == 0
                except OSError:
                    generated_ok = False
                if not generated_ok:
                    gates['web'] = dict(status='FAIL', reason='Fixture generation failed')
                else:
                    gates['web'] = run([args.node, str(ROOT / 'core/test_webui.cjs'),
                                        str(fixture)], args.output_dir, 'web')
    passed = all(row['status'] == 'PASS' for row in gates.values())
    summary = dict(status='PASS' if passed else 'INCOMPLETE_OR_FAILED',
                   python=sys.executable, repository=str(ROOT), gates=gates,
                   scope='offline host/native/editor logic',
                   firmware_build='Separate ESP-IDF gate; see docs/development.md',
                   hardware_validation=False, vehicle_validation=False)
    (args.output_dir / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n',
                                                encoding='utf-8')
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
