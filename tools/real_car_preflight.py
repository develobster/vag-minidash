"""Offline only: python tools/real_car_preflight.py --config <private config>."""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from minidash_live.validation import preflight


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config')
    parser.add_argument('--artifact-dir', default='.local/real-car')
    parser.add_argument('--uds-only', action='store_true',
                        help='Check a diagnostic-only attempt; does not enable passive CAN')
    args = parser.parse_args()
    result = preflight(args.config, args.artifact_dir, require_passive=not args.uds_only)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 1 if result['blockers'] else 0


if __name__ == '__main__':
    raise SystemExit(main())
