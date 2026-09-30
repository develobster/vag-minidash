"""Typed import recovery: synthetic values only, no live decoder evidence."""
import copy
import json
import struct
import tempfile
import unittest
from pathlib import Path

from minidash_live.mcd_import import import_mcd_mwb
from minidash_live.profiles import decode, load_profile, validate_codec
from minidash_live.registry import SignalRegistry

ROOT = Path(__file__).resolve().parents[1]


def codec(**changes):
    return dict(dict(length=1, byteorder='big', signed=False, scale=1, offset=0,
                     source='synthetic_unit_test'), **changes)


class ScalarCodecTests(unittest.TestCase):
    def test_shared_validator_rejects_invalid_representations(self):
        bad = [codec(representation='float32'), codec(length=4, signed=True, representation='float32'),
               codec(representation='unknown'), codec(scale=float('nan')), codec(signed=1),
               codec(length=True), codec(byteorder='unknown'), codec(source=''),
               codec(representation='texttable', choices=[]),
               codec(representation='texttable', scale=2, choices=[{'lower':0,'upper':0,'label':'ZERO'}])]
        for item in bad:
            with self.subTest(codec=item), self.assertRaises(ValueError):
                validate_codec(item)

    def test_texttable_rejects_overlap_out_of_range_bool_or_empty_label(self):
        for choices in ([{'lower':0,'upper':2,'label':'A'},{'lower':2,'upper':3,'label':'B'}],
                        [{'lower':-1,'upper':0,'label':'A'}],
                        [{'lower':0,'upper':256,'label':'A'}],
                        [{'lower':True,'upper':1,'label':'A'}],
                        [{'lower':0,'upper':0,'label':' '}],
                        [{'lower':2,'upper':2,'label':'B'},{'lower':0,'upper':0,'label':'A'}]):
            with self.subTest(choices=choices), self.assertRaises(ValueError):
                validate_codec(codec(representation='texttable', choices=choices))

    def test_float32_nonfinite_and_unverified_identity_are_hidden(self):
        item = {'did':'2201','decoder': codec(length=4, representation='float32'), 'unit':'bar'}
        for number in (float('nan'), float('inf'), -float('inf')):
            result = decode(item, struct.pack('>f', number))
            self.assertIsNone(result['value'])
            self.assertEqual(result['quality'], 'invalid')
        result = decode(item, struct.pack('>f', 12.5), identity_verified=False)
        self.assertIsNone(result['value'])
        self.assertEqual(result['quality'], 'identity_unverified')

    def test_legacy_profile_default_unsigned_remains_compatible(self):
        unsigned = codec()
        unsigned.pop('signed')
        document = {'schema_version':1,'id':'synthetic_profile','dids':[
            {'did':'2201','status':'candidate','decoder':unsigned}]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'profile.json'
            path.write_text(json.dumps(document), encoding='utf-8')
            loaded = load_profile(path)
        self.assertEqual(decode(loaded['dids'][0], b'\xff')['value'], 255)

    def test_importer_rejects_unknown_byte_order_and_default_texttable(self):
        records = json.loads((ROOT/'tests/fixtures/mcd-typed-scalars.json').read_text(encoding='utf-8'))
        for item, change in ((records[0], {'endianness':'unknown'}),
                             (records[2], {'compu_default_value':'UNKNOWN'})):
            record = copy.deepcopy(item)
            record['structure']['parameters'][0]['dop'].update(change)
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory)/'synthetic.json'
                path.write_text(json.dumps([record]), encoding='utf-8')
                report = import_mcd_mwb(path, ecu='fixture', provider_id='fixture')
            self.assertEqual(len(report.definitions), 0)
            self.assertEqual(len(report.unsupported), 1)

    def test_signed_invalid_sentinel_and_text_choices_stay_candidate(self):
        records = json.loads((ROOT/'tests/fixtures/mcd-typed-scalars.json').read_text(encoding='utf-8'))
        record = copy.deepcopy(records[2])
        dop = record['structure']['parameters'][0]['dop']
        dop.update(coded_base_data_type='A_INT32', encoding='2C',
                   compu_scales=[{'lower_limit':-1,'upper_limit':-1,'long_name':'REVERSE'},
                                 {'lower_limit':0,'upper_limit':0,'long_name':'NEUTRAL'}],
                   internal_constraint={'scale_constraints':[{'validity':'INVALID',
                      'lower_limit':{'type':'CLOSED','value':-128},
                      'upper_limit':{'type':'CLOSED','value':-128}}]})
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'synthetic.json'
            path.write_text(json.dumps([record]), encoding='utf-8')
            report = import_mcd_mwb(path, ecu='fixture', provider_id='fixture')
        self.assertEqual(len(report.definitions), 1)
        definition = report.definitions[0]
        self.assertIn('80', definition.decoder['invalid_raw'])
        self.assertFalse(definition.provenance.vehicle_tested)
        registry = SignalRegistry(report.definitions)
        result = registry.decode_uds('fixture', 0x2203, b'\xff', {})
        self.assertEqual((result['value'],result['quality']), ('REVERSE','candidate'))
        self.assertEqual(registry.decode_uds('fixture',0x2203,b'\x80',{})['quality'],'invalid')
