"""Export repeatable fit-test shells. Requires OpenSCAD and trimesh, never firmware."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/local/enclosures'

def main():
    import trimesh
    OUT.mkdir(parents=True, exist_ok=True)
    results = []
    for device in ('s3', 'cyd', 'lite'):
        for variant in ('compact', 'hood', 'panel'):
            for part in ('base', 'bezel'):
                path = OUT / f'{device}-{variant}-{part}.stl'
                command = ['openscad', '-o', str(path)]
                for key, value in (('device', device), ('variant', variant), ('part', part)):
                    command += ['-D', f'{key}="{value}"']
                command += [str(ROOT / 'hardware/enclosures/minidash.scad')]
                subprocess.run(command, check=True, capture_output=True, text=True)
                mesh = trimesh.load_mesh(path)
                row = dict(file=path.name, watertight=bool(mesh.is_watertight),
                           winding_consistent=bool(mesh.is_winding_consistent),
                           bodies=len(mesh.split()), volume_mm3=float(mesh.volume),
                           bounds_mm=mesh.bounds.tolist())
                if not row['watertight'] or not row['winding_consistent'] or row['bodies'] != 1 or row['volume_mm3'] <= 0:
                    raise RuntimeError(f'Invalid solid: {row}')
                results.append(row)
                print(path.name, 'PASS', flush=True)
    subprocess.run(['openscad', '-o', str(OUT / 'fit-coupon.stl'), '-D', 'part="coupon"',
                    str(ROOT / 'hardware/enclosures/minidash.scad')], check=True, capture_output=True)
    coupon = trimesh.load_mesh(OUT / 'fit-coupon.stl')
    assert coupon.is_watertight and coupon.is_winding_consistent and len(coupon.split()) == 2
    (OUT / 'mesh-validation.json').write_text(json.dumps(results, indent=2) + '\n')

if __name__ == '__main__':
    main()
