"""Private first-car recording, stage markers, sanitization and production replay."""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from minidash_live.validation import marker, sanitize_trace, compare_buttons, private_directory, cross_check
from minidash_live.validation_session import run_session, TARGETS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    for name in ('record', 'replay'):
        command = commands.add_parser(name)
        command.add_argument('--config', required=True)
        command.add_argument('--output', required=True)
        command.add_argument('--signal', action='append', choices=TARGETS, default=[])
        if name == 'replay':
            command.add_argument('--trace', required=True)
    command = commands.add_parser('marker')
    command.add_argument('name')
    command.add_argument('--directory', required=True)
    command.add_argument('--label', default='')
    command = commands.add_parser('sanitize')
    command.add_argument('--trace', required=True)
    command.add_argument('--output', required=True)
    command = commands.add_parser('compare-buttons')
    command.add_argument('--trace', required=True)
    command.add_argument('--markers', required=True)
    command.add_argument('--seconds', type=float, default=2)
    command.add_argument('--output', required=True)
    command = commands.add_parser('reference')
    command.add_argument('--trace', required=True)
    command.add_argument('--signal', required=True)
    command.add_argument('--value', type=float, required=True)
    command.add_argument('--unit', required=True)
    command.add_argument('--source', required=True)
    command.add_argument('--tolerance', type=float, required=True)
    command.add_argument('--output', required=True)
    args = parser.parse_args()
    try:
        if args.command == 'marker':
            result = marker(args.directory, args.name, args.label)
        elif args.command == 'sanitize':
            result = dict(records=sanitize_trace(args.trace, args.output),
                          manual_privacy_review_required=True, replayable=False)
        elif args.command in ('compare-buttons', 'reference'):
            result = compare_buttons(args.trace, args.markers, seconds=args.seconds) \
                if args.command == 'compare-buttons' else cross_check(
                    args.trace, args.signal, args.value, args.unit, args.source, args.tolerance)
            output = Path(args.output)
            private_directory(output.parent)
            with output.open('x', encoding='utf-8') as target:
                target.write(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
        else:
            result = run_session(args.config, args.output, signals=args.signal,
                                 replay=args.trace if args.command == 'replay' else None)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 1 if isinstance(result, dict) and result.get('status') == 'STOP' else 0
    except (OSError, ValueError, TypeError) as exc:
        print('STOP: ' + str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
