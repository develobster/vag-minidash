"""Synthetic offline gateway hints; no vehicle compatibility is established."""
import json
import unittest
from minidash_live.discovery import (
    DiscoveredEcu, DiscoverySnapshot, GatewayEndpointHint, IdentityProbe,
    parse_gateway_bitmap, plan_gateway_hints, verify_discovered_identity,
)
from minidash_live.identity import EcuIdentity, VehicleIdentity


def bitmap(*bits):
    result = bytearray(32)
    for bit in bits:
        result[bit // 8] |= 1 << (bit % 8)
    return bytes(result)


class GatewayHintTests(unittest.TestCase):
    def test_lsb_order_boundaries_and_alternate_did(self):
        for did in (0x2A26, 0x04A3):
            self.assertEqual(parse_gateway_bitmap(did, bitmap(0, 7, 8, 255)), (0, 7, 8, 255))
            self.assertEqual(parse_gateway_bitmap(did, bitmap()), ())

    def test_unknown_did_and_malformed_payload_fail_closed(self):
        for did in (0x2A28, 0xF190, True, '2A26', None):
            with self.assertRaises(ValueError):
                parse_gateway_bitmap(did, bitmap())
        for payload in (b'', bytes(31), bytes(33), bytes(35), [0] * 32, None):
            with self.assertRaises(ValueError):
                parse_gateway_bitmap(0x2A26, payload)

    def test_multiple_candidates_only_from_explicit_mapping(self):
        # Arbitrary synthetic indices, not a mapping for a real vehicle.
        a = EcuIdentity(logical_role='fixture_a', request_id=0x600, response_id=0x650)
        b = EcuIdentity(logical_role='fixture_b', request_id=0x601, response_id=0x651)
        c = EcuIdentity(logical_role='fixture_c', request_id=0x602, response_id=0x652)
        hints = plan_gateway_hints(0x2A26, bitmap(2, 20, 255), {2: a, 20: b, 21: c})
        self.assertEqual(tuple(h.bit_index for h in hints), (2, 20))
        self.assertEqual(tuple(h.configured_endpoint for h in hints), (a, b))
        self.assertTrue(all(isinstance(h, GatewayEndpointHint) for h in hints))
        self.assertTrue(all(not isinstance(h, DiscoveredEcu) for h in hints))
        self.assertEqual(plan_gateway_hints(0x2A26, bitmap(2), {}), ())
        self.assertEqual(plan_gateway_hints(0x2A26, bitmap(), {2: a}), ())

    def test_collision_rejected_even_if_colliding_bit_is_unset(self):
        a = EcuIdentity(request_id=0x600, response_id=0x650)
        for b in (a, EcuIdentity(request_id=0x650, response_id=0x660),
                  EcuIdentity(request_id=0x610, response_id=0x650)):
            with self.assertRaisesRegex(ValueError, 'collision'):
                plan_gateway_hints(0x2A26, bitmap(1), {1: a, 2: b})

    def test_invalid_mapping_is_never_turned_into_hints(self):
        valid = EcuIdentity(request_id=0x600, response_id=0x650)
        for mapping in ({-1: valid}, {256: valid}, {True: valid}, {'1': valid},
                        {1: EcuIdentity()}, {1: '600'},
                        {1: EcuIdentity(request_id=0x800, response_id=0x650)},
                        {1: EcuIdentity(request_id=0x600, response_id=0x600)}):
            with self.assertRaises(ValueError):
                plan_gateway_hints(0x2A26, bitmap(1), mapping)

    def test_missing_gateway_does_not_create_an_observation(self):
        snapshot = DiscoverySnapshot(VehicleIdentity(), (),
                                     ({'role': 'gateway', 'error': 'timeout'},))
        restored = DiscoverySnapshot.from_mapping(json.loads(json.dumps(snapshot.as_dict())))
        self.assertEqual(restored.ecus, ())
        self.assertEqual(restored.contexts(), {})
        self.assertEqual(restored.unavailable[0]['error'], 'timeout')

    def test_snapshot_reload_requires_live_identity_reverification(self):
        identity = EcuIdentity(logical_role='fixture', request_id=0x600,
                               response_id=0x650, part_number='SYNTHETIC')
        observed = DiscoveredEcu('fixture', identity, identity.fingerprint(),
                                ({'did': 'F187', 'target': 'ecu.part_number',
                                  'value': 'SYNTHETIC'},), ())
        snapshot = DiscoverySnapshot(VehicleIdentity(), (observed,))
        restored = DiscoverySnapshot.from_mapping(json.loads(json.dumps(snapshot.as_dict())))
        probe = (IdentityProbe(0xF187, 'ecu.part_number'),)
        class Client:
            def __init__(self, result):
                self.result, self.reads = result, []
            def read_did(self, did):
                self.reads.append(did)
                if isinstance(self.result, Exception):
                    raise self.result
                return self.result
        matching = Client(b'SYNTHETIC')
        self.assertTrue(verify_discovered_identity(matching, restored.ecus[0], probe))
        self.assertEqual(matching.reads, [0xF187])
        self.assertFalse(verify_discovered_identity(Client(b'REPLACED'), restored.ecus[0], probe))
        self.assertFalse(verify_discovered_identity(Client(ValueError('unavailable')),
                                                   restored.ecus[0], probe))
        # Configured endpoint or bitmap hint has no observed identity fields.
        candidate = DiscoveredEcu('fixture', identity, identity.fingerprint(), (), ())
        unused = Client(b'SYNTHETIC')
        self.assertFalse(verify_discovered_identity(unused, candidate, probe))
        self.assertEqual(unused.reads, [])


if __name__ == '__main__':
    unittest.main()
