import json
import tempfile
import unittest
from pathlib import Path
from catalog import load_vehicle, load_catalog, assess, byte_diff, ROOT


class CatalogueTests(unittest.TestCase):
    def inventory(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'inventory.json'
            path.write_text(json.dumps(data))
            return load_vehicle(path)

    def test_universal_denies_all(self):
        vehicle = load_vehicle(ROOT / 'profiles/universal.json')
        self.assertTrue(all(not assess(f, vehicle)['executable'] for f in load_catalog()))

    def test_full_identity_still_not_authority(self):
        vehicle = self.inventory({'schema':1,'name':'Golf','platform':'MQB','live_verified':True,
            'ecus':[{'address':'17','part_number':'test','hardware':'test','software':'test','dataset':'test'}]})
        self.assertFalse(vehicle['live_verified'])
        self.assertFalse(assess(load_catalog()[0], vehicle)['executable'])

    def test_duplicate_address(self):
        with self.assertRaises(ValueError):
            self.inventory({'schema':1,'name':'x','platform':'MQB','ecus':[{'address':'01'},{'address':'01'}]})

    def test_invalid_types(self):
        for ecus in [None, '01', [None], [{'address':'zz'}], [{'address':'01','software':2}]]:
            with self.assertRaises(ValueError):
                self.inventory({'schema':1,'name':'x','platform':'MQB','ecus':ecus})

    def test_catalog_integrity(self):
        features = load_catalog()
        self.assertEqual(len({f['id'] for f in features}), len(features))
        self.assertTrue(all(f['requirements'] and f['evidence'] for f in features))

    def test_byte_diff(self):
        self.assertEqual(byte_diff('AA 00','AA 81'),
            [{'byte':1,'before':'00','after':'81','changed_bits_lsb0':[0,7]}])
        self.assertEqual(byte_diff('01','01'), [])

    def test_bad_diff(self):
        for a,b in [('', ''), ('01','0102'), ('zz','01'), ('00'*256,'00'*256)]:
            with self.assertRaises(ValueError): byte_diff(a,b)
