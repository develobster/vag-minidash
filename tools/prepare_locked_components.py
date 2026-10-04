"""Fetch the committed ESP-IDF dependency lock without running a version solver.

Run with the ESP-IDF Python environment (idf-component-manager 2.5.2).
Produces a NEW EXTRA_COMPONENT_DIRS directory for IDF_COMPONENT_MANAGER=0 builds.
No firmware/lockfile edits, flashing, or vehicle access.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

from idf_component_tools.hash_tools.validate import validate_hash_eq_hashdir
from idf_component_tools.lock import LockManager
from idf_component_tools.sources.fetcher import ComponentFetcher


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', type=Path, default=Path('firmware/dependencies.lock'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        parser.error('Output must be a new directory; existing files are preserved')
    lock_bytes = args.lock.read_bytes()
    solution = LockManager(str(args.lock.resolve())).load()
    components = []
    names = set()
    for component in solution.dependencies:
        if component.name == 'idf':
            continue
        if component.source.type != 'service' or not component.component_hash:
            parser.error(f'Only hash-pinned registry components are supported: {component.name}')
        short = component.name.split('/')[-1]
        if short in names or not short.replace('_', '').isalnum():
            parser.error(f'Unsafe or ambiguous local component name: {component.name}')
        names.add(short)
        components.append((component, short))
    if not components:
        parser.error('Lockfile contains no registry dependencies')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Publish only after every original package hash has been verified.
    with tempfile.TemporaryDirectory(prefix='minidash-components-', dir=output.parent) as temporary:
        staging = Path(temporary)
        rows = []
        for component, short in components:
            print(f'Fetching {component.name}=={component.version}', flush=True)
            fetched = ComponentFetcher(component, staging).download()
            if not fetched:
                raise RuntimeError(f'No package downloaded: {component.name}')
            validate_hash_eq_hashdir(fetched, component.component_hash)
            Path(fetched).rename(staging / short)
            rows.append(dict(name=component.name, version=str(component.version),
                             component_hash=component.component_hash, directory=short))
        if args.lock.read_bytes() != lock_bytes:
            raise RuntimeError('Lockfile changed during preparation')
        manifest = dict(lock_sha256=hashlib.sha256(lock_bytes).hexdigest(), components=rows)
        (staging / 'minidash-dependencies.json').write_text(
            json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        staging.rename(output)
    print(f'Prepared and verified {len(rows)} locked components: {output}')


if __name__ == '__main__':
    main()
