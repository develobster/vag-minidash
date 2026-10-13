"""Compile the real UI with its build guard; prove the old regression is rejected.

Requires an existing ESP-IDF build and installed compiler. Never opens hardware
or modifies the configured build's objects, sdkconfig, or firmware images.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def compile_source(entry, source, output):
    command = re.sub(r' -o \S+', lambda _: ' -o "' + str(output) + '"',
                     entry['command'], count=1)
    command = re.sub(r' -c .*$', lambda _: ' -c "' + str(source) + '"',
                     command, count=1)
    # Some generators emit dependency files; keep those in this temporary run.
    command = re.sub(r' -MF \S+', lambda _: ' -MF "' + str(output.with_suffix('.d')) + '"',
                     command, count=1)
    arguments = command if os.name == 'nt' else shlex.split(command)
    return subprocess.run(arguments, cwd=entry['directory'], capture_output=True,
                          text=True, timeout=120)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument('--build-dir', type=Path,
                        default=root / 'firmware/build-can-diagnostic')
    parser.add_argument('--broken-ref', default='751d2f8')
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    commands = json.loads((args.build_dir / 'compile_commands.json').read_text())
    entry = next(item for item in commands if Path(item['file']).name == 'display_app.c')
    if '-Werror=stack-usage=3072' not in entry['command']:
        raise SystemExit('FAIL: rebuild first; the automatic UI stack guard is missing')
    baseline = subprocess.run(
        ['git', 'show', f'{args.broken_ref}:firmware/main/display_app.c'],
        cwd=root, capture_output=True, text=True, check=True).stdout
    with tempfile.TemporaryDirectory(prefix='minidash-stack-') as directory:
        temporary = Path(directory)
        fixed = compile_source(entry, Path(entry['file']), temporary / 'fixed.o')
        if fixed.returncode:
            raise SystemExit('FAIL: fixed UI rejected\n' + fixed.stdout + fixed.stderr)
        old_source = temporary / 'before.c'
        old_source.write_text(baseline, encoding='utf-8')
        broken = compile_source(entry, old_source, temporary / 'broken.o')
        errors = broken.stdout + broken.stderr
        if not broken.returncode or 'stack usage' not in errors or '-Werror=stack-usage=' not in errors:
            raise SystemExit('FAIL: original regression was not rejected by the stack guard\n' + errors)
        frames = {}
        for line in (temporary / 'fixed.su').read_text().splitlines():
            location, size, kind = line.split('\t')
            name = location.rsplit(':', 1)[-1]
            if name in {'ui_task', 'diagnostics_snapshot'}:
                frames[name] = int(size)
        if set(frames) != {'ui_task', 'diagnostics_snapshot'}:
            raise SystemExit('FAIL: expected UI frame evidence missing')
        result = {'fixed_compilation': 'PASS', 'broken_ref': args.broken_ref,
                  'broken_compilation': 'REJECTED_AS_EXPECTED',
                  'automatic_frame_limit_bytes': 3072,
                  'measured_frames_bytes': frames,
                  'limitations': 'Own-frame guard, not a complete call-graph stack proof.'}
        print(json.dumps(result, indent=2))
        if args.json:
            args.json.parent.mkdir(parents=True, exist_ok=True)
            args.json.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
