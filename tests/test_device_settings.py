import copy
import json
import tempfile
import time
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from minidash_live.api import State, serve
from minidash_live.configurator import Configurator, SimulationState, defaults, validate_config
from minidash_live.device_settings import WindowsSecretStore

class DeviceSettingsTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory()
        self.state=SimulationState();self.state.step()
        self.app=Configurator(self.state,self.directory.name)
    def tearDown(self):self.directory.cleanup()
    def body(self):return self.app.configuration()
    def test_v1_migration_preserves_screens_themes_and_preferences(self):
        config=defaults();config['schema_version']=1;config.pop('device_settings')
        config['settings']['vehicle_profile']='Existing vehicle'
        result=validate_config(config)
        self.assertEqual(result['schema_version'],2)
        self.assertEqual(result['screens'],config['screens']);self.assertEqual(result['themes'],config['themes'])
        self.assertEqual(result['device_settings']['vehicle']['profile'],'Existing vehicle')
        self.assertEqual(config['schema_version'],1)
    def test_unsafe_rates_read_only_and_static_ip_rejected(self):
        for section,key,value in [('acquisition','uds_rate_hz',50),('display','brightness',101),('vehicle','read_only',False),('wifi','station_ip','999.1.1.1'),('power','mode','hardware_off')]:
            config=defaults();config['device_settings'][section][key]=value
            with self.assertRaises(ValueError):validate_config(config)
    def test_default_update_rate_requires_sufficient_stale_window(self):
        config=defaults();config['device_settings']['acquisition']['default_rate_hz']=.2
        with self.assertRaises(ValueError):validate_config(config)
    def test_legacy_backup_preserves_new_device_preferences(self):
        self.app.config['device_settings']['wifi']['ap_ssid']='Existing protected network'
        self.app.config['device_settings']['general']['device_name']='Keep new setting'
        legacy=defaults();legacy['schema_version']=1;legacy.pop('device_settings')
        normalized=self.app.normalize_configuration(legacy)
        self.assertEqual(normalized['device_settings']['wifi']['ap_ssid'],'Existing protected network')
        self.assertEqual(normalized['device_settings']['general']['device_name'],'Keep new setting')
    def test_unavailable_capability_cannot_be_bypassed_by_put(self):
        live=Configurator(State(),self.directory.name)
        body=live.configuration();body['configuration']['device_settings']['display']['brightness']=50
        with self.assertRaises(ValueError):live.save(body)
        self.assertEqual(live.config['device_settings']['display']['brightness'],75)
    def test_general_settings_are_persisted_atomically(self):
        body=self.body();body['configuration']['device_settings']['general']['device_name']='My MiniDash'
        self.app.apply_settings(body)
        restored=Configurator(self.state,self.directory.name)
        self.assertEqual(restored.config['device_settings']['general']['device_name'],'My MiniDash')
        self.assertEqual(restored.revision,1)
    def test_reboot_requires_confirmation_and_reports_lifecycle(self):
        body=self.body();body['configuration']['device_settings']['display']['rotation']=90
        plan=self.app.plan_settings(body);self.assertTrue(plan['reboot_required'])
        with self.assertRaises(ValueError):self.app.apply_settings(body)
        body['confirmed']=True;result=self.app.apply_settings(body)
        self.assertEqual(result['operation']['status'],'running')
        self.assertTrue(result['operation']['simulation'])
        time.sleep(.8);self.assertEqual(self.app.operation['status'],'complete')
        self.assertEqual(self.app.boot_count,1)
    @unittest.skipUnless(WindowsSecretStore.available,'Windows DPAPI required')
    def test_wifi_password_write_only_protected_and_backup_secret_free(self):
        secret='wifi-test-only-93'
        body=self.body();body['secrets']={'ap_password':secret};body['confirmed']=True
        body['configuration']['device_settings']['wifi']['ap_ssid']='MiniDash-test'
        plan=self.app.plan_settings(body);self.assertTrue(plan['reconnect_required']);self.assertNotIn(secret,json.dumps(plan))
        response=self.app.apply_settings(body)
        self.assertTrue(response['secret_status']['ap_password'])
        for value in (response,self.app.configuration(),self.app.telemetry(),self.app.diagnostic_bundle()):self.assertNotIn(secret,json.dumps(value))
        disk=(Path(self.directory.name)/'configuration.json').read_text(encoding='utf-8');self.assertNotIn(secret,disk)
        restored=Configurator(self.state,self.directory.name);self.assertEqual(restored._secrets['ap_password'],secret)
        time.sleep(1.2)
        next_body=self.app.configuration();next_body['configuration']['device_settings']['general']['device_name']='Retain credential'
        self.app.apply_settings(next_body)
        self.assertTrue(self.app.secret_status()['ap_password'])
    def test_password_in_ordinary_backup_rejected(self):
        config=defaults();config['device_settings']['wifi']['ap_password']='do-not-export'
        with self.assertRaises(ValueError):validate_config(config)
        config=defaults();config['screens'][0]['password']='hidden'
        with self.assertRaises(ValueError):validate_config(config)
    def test_diagnostic_bundle_scrubs_runtime_keys_and_known_secret_values(self):
        self.app._secrets={'ap_password':'redact-me-123'}
        self.app.state.snapshot=lambda:json.dumps(dict(status='test',signals=[dict(signal='test.signal',value=None,quality='error',password='redact-me-123',source_metadata={'token':'123'},error='contains redact-me-123')])).encode()
        bundle=json.dumps(self.app.diagnostic_bundle())
        self.assertNotIn('redact-me-123',bundle);self.assertNotIn('"password"',bundle);self.assertNotIn('"token"',bundle)
    def test_factory_reset_requires_full_device_id_and_forgets_credentials(self):
        body=dict(action='factory_reset',confirmed=True,revision=0)
        with self.assertRaises(ValueError):self.app.system_action(body)
        self.app._secrets={'ap_password':'discard-this-123'}
        body['device_id_confirmation']=self.app.device_id
        response=self.app.system_action(body)
        self.assertFalse(response['secret_status']['ap_password'])
        self.assertEqual(self.app.revision,1)
    def test_selected_logging_and_disabled_logging_are_real(self):
        body=self.body();logging=body['configuration']['device_settings']['logging'];logging.update(signal_scope='selected',selected_signals=['engine.rpm'])
        self.app.apply_settings(body);self.app.start_recording();self.app.sample();meta=self.app.stop_recording()
        self.assertEqual(meta['signals'],['engine.rpm'])
        body=self.body();body['configuration']['device_settings']['logging']['enabled']=False;self.app.apply_settings(body)
        with self.assertRaises(ValueError):self.app.start_recording()
    def test_simulated_sleep_stops_publishing_but_is_not_hardware_off(self):
        body=self.body();body['configuration']['device_settings']['power']['mode']='deep_sleep';body['confirmed']=True
        self.app.apply_settings(body)
        before=self.state.store.snapshot()[0]['timestamp'];self.state.step()
        self.assertEqual(self.state.store.snapshot()[0]['timestamp'],before)
        self.assertFalse(self.app.device_capabilities()['power']['hardware_cutoff'])
        time.sleep(.8)

class SettingsApiTests(unittest.TestCase):
    def test_capabilities_scan_apply_and_redacted_gets(self):
        with tempfile.TemporaryDirectory() as directory:
            state=SimulationState();state.step();server,thread=serve(state,0,data_dir=directory)
            base=f'http://127.0.0.1:{server.server_port}'
            def call(path,method='GET',body=None):
                req=Request(base+path,method=method,data=json.dumps(body).encode() if body is not None else None,headers={'Content-Type':'application/json'})
                with urlopen(req,timeout=3) as r:return json.load(r)
            try:
                self.assertEqual(call('/api/settings/schema')['schema_version'],2)
                self.assertEqual(call('/api/capabilities')['features']['wifi_scan']['scope'],'simulation')
                self.assertEqual(call('/api/wifi/scan','POST',{})['scope'],'simulation')
                payload=call('/api/configuration');payload['configuration']['device_settings']['general']['device_name']='API settings'
                self.assertFalse(call('/api/settings/plan','POST',payload)['reconnect_required'])
                call('/api/settings/apply','POST',payload)
                self.assertEqual(call('/api/diagnostics/bundle')['configuration']['device_settings']['general']['device_name'],'API settings')
            finally:server.shutdown();thread.join(2);server.server_close()

if __name__=='__main__':unittest.main()
