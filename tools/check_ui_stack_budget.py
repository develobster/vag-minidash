"""Fail normal UI builds when measured startup/diagnostic paths exceed the budget.

The selected paths use GCC frame reports and a 1 KiB reserve. This is a guard
against known regressions, not a full program call-graph stack proof.
"""
import argparse
from pathlib import Path
import re


def evaluate(frames, stack_bytes, reserve_bytes=1024):
    paths = {
        'UI diagnostics': ('ui_task', 'diagnostics_snapshot'),
        'UI saved configuration': ('ui_task', 'md_configuration_storage_load',
                                   'md_configuration_decode', 'md_configuration_validate'),
    }
    measurements = {}
    for label, names in paths.items():
        missing = set(names) - frames.keys()
        if missing:
            raise ValueError(f'{label}: missing frame reports for {sorted(missing)}')
        measured = sum(frames[name] for name in names)
        if measured + reserve_bytes > stack_bytes:
            raise ValueError(f'{label}: frames {measured} + reserve {reserve_bytes} '
                             f'> task stack {stack_bytes} bytes')
        measurements[label] = measured
    return measurements


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--stack-bytes', type=int, required=True)
    args = parser.parse_args()
    config = (args.build_dir / 'config/sdkconfig.h').read_text()
    if not re.search(r'^#define CONFIG_MINIDASH_DISPLAY_ENABLE 1$', config, re.M) or re.search(
            r'^#define CONFIG_MINIDASH_DISPLAY_SMOKE_TEST 1$', config, re.M):
        print('UI stack paths: not active in this headless/panel-smoke build')
        return
    frames = {}
    directory = args.build_dir / 'esp-idf/main/CMakeFiles/__idf_main.dir'
    for source in ('display_app.c', 'ui_configuration.c', 'ui_configuration_nvs.c'):
        report = directory / (source + '.su')
        if not report.is_file():
            raise SystemExit(f'FAIL: missing compiler stack report: {report}')
        for line in report.read_text().splitlines():
            location, size, kind = line.split('\t')
            name = location.rsplit(':', 1)[-1]
            if kind != 'static':
                raise SystemExit(f'FAIL: unbounded/dynamic frame requires review: {name} ({kind})')
            frames[name] = int(size)
    try:
        results = evaluate(frames, args.stack_bytes)
    except ValueError as error:
        raise SystemExit(f'FAIL: {error}') from error
    for label, measured in results.items():
        print(f'PASS: {label}: frames={measured} + reserve=1024 '
              f'<= task_stack={args.stack_bytes} bytes')


if __name__ == '__main__':
    main()
