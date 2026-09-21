"""Offline VAG feature catalogue. Never authorizes or sends vehicle writes."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
IDENTITY_FIELDS = ('part_number', 'hardware', 'software', 'dataset')


def load_vehicle(path):
    path = Path(path)
    if path.stat().st_size > 1024 * 1024:
        raise ValueError('Profile is larger than 1 MiB')
    data = json.loads(path.read_text(encoding='utf8'))
    if not isinstance(data, dict) or data.get('schema') != 1:
        raise ValueError('Expected vehicle inventory schema 1')
    if not isinstance(data.get('name'), str) or not data['name'].strip():
        raise ValueError('Vehicle name required')
    if data.get('platform') not in ('unknown', 'PQ', 'MQB', 'MQB_EVO', 'MLB', 'MLB_EVO'):
        raise ValueError('Unsupported platform label')
    ecus = data.get('ecus')
    if not isinstance(ecus, list) or len(ecus) > 256:
        raise ValueError('Expected an ECU list, maximum 256 entries')
    addresses = set()
    for ecu in ecus:
        if not isinstance(ecu, dict):
            raise ValueError('ECU must be an object')
        address = ecu.get('address')
        if not isinstance(address, str) or len(address) != 2 or any(c not in '0123456789ABCDEF' for c in address):
            raise ValueError('ECU address must be two uppercase hex digits')
        if address in addresses:
            raise ValueError('Duplicate ECU address')
        addresses.add(address)
        for key in IDENTITY_FIELDS:
            if ecu.get(key) is not None and (not isinstance(ecu[key], str) or not ecu[key].strip()):
                raise ValueError(f'Invalid identity field: {key}')
        equipment = ecu.get('equipment', [])
        if not isinstance(equipment, list) or not all(isinstance(v, str) for v in equipment):
            raise ValueError('Equipment must be a list of strings')
    # User supplied flags can never promote a profile to a verified/live one.
    return {'schema': 1, 'name': data['name'], 'platform': data['platform'],
            'ecus': ecus, 'source': 'user_inventory', 'live_verified': False}


def load_catalog():
    return json.loads((ROOT / 'profiles' / 'features.json').read_text(encoding='utf8'))['features']


def assess(feature, vehicle):
    reasons = []
    if feature['scope'] == 'excluded':
        reasons.append('Вне области текущего проекта')
    if vehicle['platform'] == 'unknown':
        reasons.append('Платформа не определена')
    # Addresses are research hints only, not proof of compatibility.
    candidates = [e for e in vehicle['ecus'] if e['address'] in feature['candidate_addresses']]
    if not candidates:
        reasons.append('Нет идентификации предполагаемого блока')
    elif not any(all(e.get(k) for k in IDENTITY_FIELDS) for e in candidates):
        reasons.append('Неполная идентификация HW/SW/dataset')
    reasons.append('Нет проверенного рецепта для точной версии блока')
    reasons.append('Реальный исполнитель кодирования отсутствует')
    return {'id': feature['id'], 'title': feature['title'], 'status': 'research_only',
            'executable': False, 'reasons': reasons,
            'requirements': feature['requirements']}


def byte_diff(before, after):
    """Offline equal-length hex comparison, not a CAN command or coding recipe."""
    if not isinstance(before, str) or not isinstance(after, str):
        raise ValueError('Hex strings required')
    a, b = bytes.fromhex(before), bytes.fromhex(after)
    if not a or len(a) != len(b) or len(a) > 255:
        raise ValueError('Coding must have equal lengths of 1..255 bytes')
    return [{'byte': i, 'before': f'{x:02X}', 'after': f'{y:02X}',
             'changed_bits_lsb0': [bit for bit in range(8) if (x ^ y) & (1 << bit)]}
            for i, (x, y) in enumerate(zip(a, b)) if x != y]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vehicle', default=str(ROOT / 'profiles' / 'universal.json'))
    args = parser.parse_args()
    try:
        vehicle = load_vehicle(args.vehicle)
        print(json.dumps([assess(f, vehicle) for f in load_catalog()], ensure_ascii=False, indent=2))
    except (ValueError, OSError) as exc:
        parser.exit(1, f'Invalid inventory: {exc}\n')


if __name__ == '__main__':
    main()
