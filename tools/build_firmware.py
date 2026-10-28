"""Build an isolated hardware preset in an exported ESP-IDF 5.5.5 environment."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target',choices=['minidash_cyd','esp32_s3'])
    parser.add_argument('--components',type=Path,help='Optional hash-verified local components')
    parser.add_argument('--listen-only', action='store_true', help='Build CYD with diagnostic TX disabled')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    idf=Path(os.environ['IDF_PATH'])/'tools/idf.py'
    if args.listen_only and args.target != 'minidash_cyd':
        parser.error('--listen-only is for minidash_cyd')
    build=root/f'build-{args.target}{"-listen" if args.listen_only else ""}'
    command=[sys.executable,str(idf),'-C',str(root/'firmware'),'-B',str(build),
             f'-DMINIDASH_BOARD={args.target}','-DCCACHE_ENABLE=0']
    if args.listen_only:
        command.append('-DMINIDASH_CYD_LISTEN_ONLY=ON')
    if args.target=='esp32_s3':
        command.extend([f'-DSDKCONFIG={root / "firmware/sdkconfig.s3-regression.generated"}',
                        '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.standalone'])
    if args.components:
        command.append(f'-DEXTRA_COMPONENT_DIRS={args.components.resolve()}')
    subprocess.run(command+['build'],check=True)

if __name__=='__main__':
    main()
