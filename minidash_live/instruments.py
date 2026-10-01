"""Presentation profiles only: never select diagnostic decoders or safety limits."""
import json
from pathlib import Path

CATALOG = json.loads((Path(__file__).resolve().parents[1] /
    'data/ui/instrument-profiles.json').read_text(encoding='utf8'))
PROFILES = {p['id']: p for p in CATALOG['profiles']}

def select_instrument_profile(discovery, *, market='eu'):
    """Match explicit generation/year hints plus successfully observed engine ASAM.

    VIN alone cannot establish a cluster variant. Unknown/non-EU/ambiguous inputs
    return default. Catalogue examples remain representative, not exact trim proof.
    """
    vehicle=discovery.vehicle
    if market != 'eu' or not vehicle.model or not vehicle.model_year:
        return 'default'
    try:
        year=int(vehicle.model_year)
    except (ValueError,TypeError):
        return 'default'
    engines=[e for e in discovery.ecus if e.role == 'engine']
    if len(engines) != 1 or engines[0].errors:
        return 'default'
    engine=engines[0]
    asam=engine.identity.asam_id or ''
    if not asam.startswith('EV_ECM') or not any(
        f.get('target') == 'ecu.asam_id' and str(f.get('value','')).upper() == asam
        for f in engine.fields):
        return 'default'
    fuels=[fuel for fuel in ('TFS','TDI') if fuel in asam]
    if len(fuels) != 1:
        return 'default'
    manufacturer=(vehicle.manufacturer or '').replace('Š','S')
    matches=[p['id'] for p in PROFILES.values() if manufacturer == p['manufacturer'] and
        vehicle.model in p['models'] and p['years'][0] <= year <= p['years'][1] and
        p['fuel'] == fuels[0]]
    return matches[0] if len(matches) == 1 else 'default'

def encode_instrument_profile(profile_id, ttl_ms=3000):
    if profile_id != 'default' and profile_id not in PROFILES:
        raise ValueError('Unknown instrument profile')
    if type(ttl_ms) is not int or not 1 <= ttl_ms <= 60000:
        raise ValueError('Invalid instrument profile TTL')
    return f'MDV1|{profile_id}|{ttl_ms}\n'.encode('ascii')
