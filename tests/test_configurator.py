import copy
import json
import tempfile
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from minidash_live.api import State, serve
from minidash_live.configurator import Configurator, SimulationState, defaults, validate_config

class ConfigurationTests(unittest.TestCase):
    def test_default_and_old_version(self):
        config = defaults()
        self.assertEqual(validate_config(config), config)
        config['schema_version'] = 0
        with self.assertRaises(ValueError): validate_config(config)

    def test_import_rejects_overlaps_and_unsafe_theme(self):
        config = defaults()
        config['screens'][0]['widgets'][1]['x'] = 0
        with self.assertRaises(ValueError): validate_config(config)
        config = defaults(); config['themes'][0]['accent'] = 'red; background:url(x)'
        with self.assertRaises(ValueError): validate_config(config)

    def test_nonfinite_threshold_geometry_and_duplicate_ids(self):
        for key, value in [('warning',float('nan')),('precision',1.5),('width',5),('max',-1)]:
            config=defaults(); config['screens'][0]['widgets'][0][key]=value
            with self.assertRaises(ValueError): validate_config(config)
        config=defaults(); config['themes'].append(copy.deepcopy(config['themes'][0]))
        with self.assertRaises(ValueError): validate_config(config)

    def test_save_survives_restart_and_rejects_conflict(self):
        with tempfile.TemporaryDirectory() as directory:
            app=Configurator(State(),directory)
            config=app.configuration()['configuration'];config['screens'][0]['name']='Persisted'
            app.save(dict(configuration=config,revision=0))
            with self.assertRaises(RuntimeError): app.save(dict(configuration=config,revision=0))
            self.assertEqual(Configurator(State(),directory).config['screens'][0]['name'],'Persisted')

    def test_recording_files_metadata_and_source_are_real(self):
        with tempfile.TemporaryDirectory() as directory:
            state=SimulationState();state.step();app=Configurator(state,directory)
            app.start_recording();app.sample();result=app.stop_recording()
            self.assertEqual(result['samples'],1)
            rows=[json.loads(line) for line in app.log_path(result['id']).read_text(encoding='utf-8').splitlines()]
            self.assertEqual(rows[0]['mode'],'simulation')
            self.assertTrue(all(row['source']=='simulated' for row in rows[0]['signals']))
            self.assertEqual(app.recordings()[0]['id'],result['id'])
            with self.assertRaises(ValueError): app.log_path('../configuration')

    def test_runtime_adapter_preserves_arbitration_and_stales_if_producer_stops(self):
        state=State()
        state.update_runtime(dict(decisions={'engine.rpm':dict(selected=dict(signal='engine.rpm',value=1000,
            source='uds',quality='valid',ttl=3,age=2))},metrics={}),ecus=['engine'])
        with tempfile.TemporaryDirectory() as directory:
            app=Configurator(state,directory)
            row=next(r for r in app.telemetry()['signals'] if r['signal']=='engine.rpm')
            self.assertEqual(row['source'],'uds');self.assertEqual(row['value'],1000)
            with state.lock: state.data['signals']['engine.rpm']['received_at']-=4
            row=next(r for r in app.telemetry()['signals'] if r['signal']=='engine.rpm')
            self.assertEqual(row['quality'],'stale');self.assertIsNone(row['value'])

class ConfiguratorApiTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory()
        self.state=SimulationState();self.state.step()
        self.server,self.thread=serve(self.state,0,data_dir=self.directory.name)
        self.url=f'http://127.0.0.1:{self.server.server_port}'
    def tearDown(self):
        self.server.shutdown();self.thread.join(2);self.server.server_close();self.directory.cleanup()
    def request(self,path,method='GET',body=None,headers=None):
        with urlopen(Request(self.url+path,method=method,data=json.dumps(body).encode() if body is not None else None,
                           headers=headers or ({'Content-Type':'application/json'} if body is not None else {})),timeout=3) as r:
            return json.load(r)
    def test_validation_is_nonmutating_and_failed_import_keeps_configuration(self):
        original=self.request('/api/configuration')
        candidate=copy.deepcopy(original['configuration']);candidate['screens'][0]['name']='Imported'
        self.request('/api/configuration/validate','POST',dict(configuration=candidate))
        self.assertEqual(self.request('/api/configuration'),original)
        candidate['schema_version']=2
        with self.assertRaises(HTTPError) as err:self.request('/api/configuration','PUT',dict(configuration=candidate,revision=0))
        self.assertEqual(err.exception.code,400);self.assertEqual(self.request('/api/configuration'),original)
    def test_configuration_conflicts_return_409(self):
        payload=self.request('/api/configuration');self.request('/api/configuration','PUT',payload)
        with self.assertRaises(HTTPError) as err:self.request('/api/configuration','PUT',payload)
        self.assertEqual(err.exception.code,409)
    def test_cross_origin_write_is_rejected(self):
        with self.assertRaises(HTTPError) as err:
            self.request('/api/logs/start','POST',{},headers={'Content-Type':'application/json','Origin':'http://foreign.example'})
        self.assertEqual(err.exception.code,403)
    def test_sse_and_no_cached_telemetry(self):
        with urlopen(self.url+'/api/events',timeout=3) as r:
            self.assertEqual(r.headers['Cache-Control'],'no-store')
            line=r.readline().decode();self.assertTrue(line.startswith('data: '))
            self.assertEqual(json.loads(line[6:])['mode'],'simulation')
    def test_ota_is_explicitly_unavailable(self):
        self.assertFalse(self.request('/api/ota')['available'])
    def test_recording_download_delete_and_traversal(self):
        self.request('/api/logs/start','POST',{})
        self.server.configurator.sample()
        metadata=self.request('/api/logs/stop','POST',{})
        with urlopen(self.url+'/api/logs/'+metadata['id']) as r:self.assertIn(b'simulated',r.read())
        self.request('/api/logs/'+metadata['id'],'DELETE',{})
        self.assertEqual(self.request('/api/logs'),[])
        with self.assertRaises(HTTPError):self.request('/api/logs/../../configuration')

if __name__=='__main__':unittest.main()
