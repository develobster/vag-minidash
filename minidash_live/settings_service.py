"""Settings application lifecycle on the host; explicit simulation of hardware effects."""
from copy import deepcopy
from datetime import datetime, timezone
import json
from pathlib import Path
import threading
import time
import uuid
from .device_settings import (FIELDS, WindowsSecretStore, capabilities, change_plan,
                              validate_credentials, settings_defaults)

class SettingsService:
    def _init_settings(self,saved):
        self.device_id=(saved or {}).get('device_id') or uuid.uuid5(uuid.NAMESPACE_URL,str(self.directory.resolve())).hex
        self.vault=WindowsSecretStore
        self._secrets=self.vault.unseal((saved or {}).get('protected_secrets'))
        self.operation=None
        self.recent_events=[]
        self.wifi_state='not_configured'
        self.boot_count=0
        self._sync_settings()
        if not self.config['device_settings']['general']['remember_last_screen']:
            self.runtime_screen=self.config['device_settings']['general']['startup_screen']

    def device_capabilities(self):
        return capabilities(getattr(self.state,'mode','research'),self.vault.available)

    def settings_schema(self):
        return dict(schema_version=2,sections=deepcopy(FIELDS),capabilities=self.device_capabilities())

    def secret_status(self):
        return {key:bool(self._secrets.get(key)) for key in ('ap_password','station_password')}

    def device_info(self):
        with self.lock:
            storage_used=sum(p.stat().st_size for p in (self.directory/'logs').glob('*.jsonl'))
        wifi=self.config['device_settings']['wifi']
        return dict(id=self.device_id,firmware_version=None,hardware_revision=None,
                    build='host-settings-v2',reset_reason='simulation_restart' if self.boot_count else 'host_start',
                    boot_count=self.boot_count,filesystem='host filesystem',
                    storage_used_bytes=storage_used,
                    flash_bytes=None,psram_bytes=None,heap_bytes=None,
                    wifi=dict(state=self.wifi_state,ap_address='192.168.4.1' if getattr(self.state,'mode',None)=='simulation' and wifi['ap_enabled'] else None,
                              station_address=wifi['station_ip'] if getattr(self.state,'mode',None)=='simulation' and wifi['station_enabled'] and wifi['station_address_mode']=='static' else None,
                              scope='simulation' if getattr(self.state,'mode',None)=='simulation' else 'unavailable'),
                    interface='canonical simulation' if getattr(self.state,'mode',None)=='simulation' else 'runtime profile',
                    read_only=True,power_state=self.config['device_settings']['power']['mode'] if getattr(self.state,'mode',None)=='simulation' else 'unknown')

    def _sync_settings(self):
        self.runtime_screen=self.config['active_screen']
        if getattr(self.state,'mode',None)=='simulation':
            self.state.device_settings=deepcopy(self.config['device_settings'])

    def _plan(self,body):
        config=self.normalize_configuration(body['configuration'])
        if type(body.get('revision')) is not int or body['revision']!=self.revision:raise RuntimeError('Configuration changed elsewhere. Reload before applying.')
        changes=body.get('secrets',{})
        if not isinstance(changes,dict):raise ValueError('Invalid secret input')
        plan=change_plan(self.config,config,self.device_capabilities(),changes)
        if any(row['path'].startswith('wifi.') for row in plan['changes']):
            validate_credentials(config['device_settings'],self._secrets,changes)
        return config,plan

    def normalize_configuration(self,configuration):
        from .configurator import validate_config
        from .device_settings import migrate_configuration
        if isinstance(configuration,dict) and configuration.get('schema_version')==1:
            migrated=migrate_configuration(configuration,self.device_id)
            migrated['device_settings']=deepcopy(self.config['device_settings'])
            migrated['device_settings']['vehicle']['profile']=configuration.get('settings',{}).get('vehicle_profile','')
            ids=[screen['id'] for screen in migrated['screens']]
            if migrated['device_settings']['general']['startup_screen'] not in ids:
                migrated['device_settings']['general']['startup_screen']=migrated['active_screen']
            return validate_config(migrated)
        return validate_config(configuration)

    def plan_settings(self,body):
        with self.lock:
            if self.operation and self.operation['status']=='running':raise RuntimeError('A device operation is still in progress')
            return self._plan(body)[1]

    def _envelope(self,config,revision,secrets=None):
        return dict(configuration=config,revision=revision,device_id=self.device_id,
                    protected_secrets=self.vault.seal(self._secrets if secrets is None else secrets))

    def apply_settings(self,body):
        from .configurator import atomic_json
        with self.lock:
            if self.operation and self.operation['status']=='running':raise RuntimeError('A device operation is still in progress')
            config,plan=self._plan(body)
            if any(plan[key] for key in ('reconnect_required','reboot_required','runtime_restart_required')) and body.get('confirmed') is not True:
                raise ValueError('Confirm the reconnect/restart impact before applying')
            secrets=validate_credentials(config['device_settings'],self._secrets,body.get('secrets',{})) if any(row['path'].startswith('wifi.') for row in plan['changes']) else self._secrets
            # One replacement commits ordinary configuration and encrypted secrets together.
            atomic_json(self.directory/'configuration.json',self._envelope(config,self.revision+1,secrets))
            self.config=config;self.revision+=1;self._secrets=secrets
            self._sync_settings()
            if self.active and not config['device_settings']['logging']['enabled']:self.stop_recording()
            operation=self._start_operation(plan)
            self.recent_events.append(dict(time=datetime.now(timezone.utc).isoformat(),kind='settings_applied',impacts=plan['impacts']))
            self.recent_events=self.recent_events[-50:]
            return {**self.configuration(),'plan':plan,'operation':operation}

    def _start_operation(self,plan,action='settings'):
        phases=['Saving settings']
        if plan.get('reconnect_required'):phases+=['Restarting simulated Wi-Fi','Waiting for MiniDash simulation','Reconnect to simulator']
        if plan.get('reboot_required'):phases+=['Rebooting simulated device','Waiting for MiniDash simulation']
        if plan.get('runtime_restart_required'):phases+=['Restarting simulated acquisition']
        self.operation=dict(id=uuid.uuid4().hex,status='running',phase=phases[0],action=action,
                            reconnect_required=plan.get('reconnect_required',False),reboot_required=plan.get('reboot_required',False),
                            simulation=getattr(self.state,'mode',None)=='simulation')
        if len(phases)==1:
            self.operation.update(status='complete',phase='Applied');return deepcopy(self.operation)
        identifier=self.operation['id']
        def run():
            for phase in phases[1:]:
                time.sleep(.35)
                with self.lock:
                    if not self.operation or self.operation['id']!=identifier:return
                    self.operation['phase']=phase
                    if 'Wi-Fi' in phase:self.wifi_state='restarting_simulation'
            with self.lock:
                if plan.get('reconnect_required'):self.wifi_state='active_simulation'
                if plan.get('reboot_required'):
                    self.boot_count+=1;self.started=time.monotonic();self.state.started=time.monotonic()
                    general=self.config['device_settings']['general']
                    self.runtime_screen=self.config['active_screen'] if general['remember_last_screen'] else general['startup_screen']
                self.operation.update(status='complete',phase='Simulation applied · connection ready')
        threading.Thread(target=run,daemon=True).start()
        return deepcopy(self.operation)

    def scan_wifi(self):
        if not self.device_capabilities()['features']['wifi_scan']['supported']:raise ValueError('Wi-Fi scan not supported by current firmware')
        return dict(scope='simulation',networks=[dict(ssid='Simulation Garage',security='wpa2',rssi=-48),dict(ssid='Simulation Guest',security='open',rssi=-65)])

    def system_action(self,body):
        from .configurator import atomic_json, defaults
        action=body.get('action');feature={'restart_device':'restart_device','restart_wifi':'restart_wifi','reset_settings':'settings_reset','factory_reset':'factory_reset'}.get(action)
        if not feature:raise ValueError('Unknown system action')
        if not self.device_capabilities()['features'][feature]['supported']:raise ValueError('Not supported by current firmware')
        if body.get('confirmed') is not True:raise ValueError('Action requires confirmation')
        with self.lock:
            if body.get('revision')!=self.revision:raise RuntimeError('Configuration changed elsewhere. Reload before resetting.')
            if self.operation and self.operation['status']=='running':raise RuntimeError('A device operation is still in progress')
            if self.active:raise RuntimeError('Stop recording before a system action')
            if action=='factory_reset' and body.get('device_id_confirmation')!=self.device_id:raise ValueError('Factory reset requires the full Device ID')
            reset_wifi=reset_device=False
            if action in ('reset_settings','factory_reset'):
                config=defaults() if action=='factory_reset' else deepcopy(self.config)
                config['device_settings']=settings_defaults(self.device_id,config['active_screen'])
                reset_wifi=getattr(self.state,'mode',None)=='simulation' and (config['device_settings']['wifi']!=self.config['device_settings']['wifi'] or action=='factory_reset' and bool(self._secrets))
                reset_device=getattr(self.state,'mode',None)=='simulation' and config['device_settings']['display']['rotation']!=self.config['device_settings']['display']['rotation']
                secrets={} if action=='factory_reset' else self._secrets
                atomic_json(self.directory/'configuration.json',self._envelope(config,self.revision+1,secrets))
                self.config=config;self.revision+=1;self._secrets=secrets;self._sync_settings()
                # Existing recordings are intentionally kept and remain separately deletable.
            plan=dict(reconnect_required=action=='restart_wifi' or reset_wifi,reboot_required=action=='restart_device' or reset_device)
            return {**self.configuration(),'operation':self._start_operation(plan,action)}

    def diagnostic_bundle(self):
        from .device_settings import reject_secrets,sanitize
        configuration=deepcopy(self.config);reject_secrets(configuration)
        return sanitize(dict(bundle_version=1,created=datetime.now(timezone.utc).isoformat(),device=self.device_info(),
                    configuration=configuration,capabilities=self.device_capabilities(),runtime=self.telemetry(),
                    recent_events=deepcopy(self.recent_events),recent_errors=[json.loads(self.state.snapshot()).get('last_error')],recordings=self.recordings()), self._secrets.values())
