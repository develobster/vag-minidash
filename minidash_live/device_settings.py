"""Versioned device settings contract and capability enforcement, independent of UI.

Simulation capabilities never imply ESP32 support. Passwords are write-only;
Windows DPAPI protects their at-rest envelope under the current user account.
"""
from copy import deepcopy
import base64
import ctypes
import ipaddress
import json
import math
import os
import re
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

SCHEMA_VERSION = 2

def field(label, default, kind='text', capability='general', impact='apply', **options):
    return dict(label=label, default=default, kind=kind, capability=capability, impact=impact, **options)

FIELDS = {
    'general': {
        'device_name': field('Device name', 'MiniDash', max_length=64),
        'locale': field('Language / locale', 'en', 'select', choices=['en','ru'], note='Locale architecture ready; interface translations are not installed'),
        'units': field('Units preset', 'metric', 'select', choices=['metric','imperial','custom']),
        'temperature': field('Temperature', '°C', 'select', choices=['°C','°F']),
        'pressure': field('Pressure', 'bar', 'select', choices=['bar','psi','kPa']),
        'speed': field('Speed', 'km/h', 'select', choices=['km/h','mph']),
        'time_format': field('Time format', '24h', 'select', choices=['24h','12h']),
        'timezone': field('Timezone', 'Europe/Minsk', max_length=64),
        'automatic_time': field('Automatic time', True, 'toggle', 'clock_sync', note='Host clock is automatic; device time synchronization requires firmware support'),
        'startup_screen': field('Startup screen', 'engine', 'screen'),
        'remember_last_screen': field('Remember last screen', True, 'toggle'),
    },
    'display': {
        'brightness': field('Brightness', 75, 'range', 'display_brightness', 'live', min=1,max=100),
        'auto_brightness': field('Auto brightness', False, 'toggle', 'auto_brightness'),
        'day_brightness': field('Day brightness', 85, 'range', 'display_brightness', min=1,max=100),
        'night_brightness': field('Night brightness', 20, 'range', 'display_brightness', min=1,max=100),
        'mode': field('Display mode', 'manual', 'select','display_brightness',choices=['auto','day','night','manual'],note='Auto uses day/night hours in the configured timezone; ambient light sensing is unavailable'),
        'rotation': field('Screen rotation', 0, 'select','display_rotation','reboot',choices=[0,90,180,270]),
        'sleep_timeout_s': field('Sleep timeout (s)', 300, 'number','power_management',min=0,max=86400),
        'screen_off_timeout_s': field('Screen off timeout (s)', 120, 'number','display_brightness',min=0,max=86400),
        'wake_behaviour': field('Display wake behaviour', 'button', 'select','power_management',choices=['button','touch','ignition']),
        'fps': field('Physical display FPS', 30,'number','display_fps',min=5,max=60),
    },
    'wifi': {
        'mode': field('Wi-Fi mode', 'ap', 'select','wifi_config','wifi_reconnect',choices=['ap','station','ap_station']),
        'ap_enabled': field('Enable access point', True, 'toggle','wifi_config','wifi_reconnect'),
        'ap_ssid': field('AP SSID', 'MiniDash-0000', capability='wifi_config',impact='wifi_reconnect',max_length=32),
        'ap_security': field('AP security', 'wpa2', 'select','wifi_config','wifi_reconnect',choices=['wpa2','open']),
        'ap_channel': field('AP channel', 6,'number','wifi_config','wifi_reconnect',min=1,max=11),
        'ap_hidden': field('Hidden AP SSID', False,'toggle','wifi_config','wifi_reconnect'),
        'ap_auto_shutdown_s': field('AP auto shutdown (s; 0 = never)', 0,'number','wifi_config','wifi_reconnect',min=0,max=86400),
        'station_enabled': field('Enable station', False,'toggle','wifi_station','wifi_reconnect'),
        'station_ssid': field('Station SSID', '', capability='wifi_station',impact='wifi_reconnect',max_length=32,allow_empty=True),
        'station_security': field('Station security','wpa2','select','wifi_station','wifi_reconnect',choices=['wpa2','open']),
        'station_address_mode': field('Station addressing', 'dhcp','select','wifi_station','wifi_reconnect',choices=['dhcp','static']),
        'station_ip': field('Static IP', '192.168.1.50','ip','wifi_station','wifi_reconnect'),
        'station_netmask': field('Netmask', '255.255.255.0','ip','wifi_station','wifi_reconnect'),
        'station_gateway': field('Gateway', '192.168.1.1','ip','wifi_station','wifi_reconnect'),
        'station_dns': field('DNS', '192.168.1.1','ip','wifi_station','wifi_reconnect'),
    },
    'vehicle': {
        'profile': field('Vehicle profile', '', allow_empty=True,max_length=120),
        'auto_detect': field('Auto detect vehicle', False,'toggle','can','runtime_restart'),
        'bitrate': field('CAN bitrate', 'auto','select','can','runtime_restart',choices=['auto',125000,250000,500000,1000000]),
        'passive_can_enabled': field('Passive CAN enabled', True,'toggle','can','runtime_restart'),
        'uds_enabled': field('UDS enabled', False,'toggle','uds','runtime_restart'),
        'discovery_enabled': field('Discovery enabled', False,'toggle','discovery','runtime_restart'),
        'read_only': field('SAFE / READ ONLY', True,'toggle','read_only'),
    },
    'acquisition': {
        'default_rate_hz': field('Default update rate (Hz)', 2,'number','acquisition','runtime_restart',min=.2,max=10,step=.1),
        'ui_refresh_hz': field('UI refresh rate (Hz)', 2,'number','ui_refresh','live',min=.5,max=5,step=.5),
        'can_rate_hz': field('CAN signal rate limit (Hz)', 20,'number','acquisition','runtime_restart',min=1,max=100),
        'uds_rate_hz': field('UDS polling rate (Hz)', 1,'number','acquisition','runtime_restart',min=.2,max=2,step=.1,note='0.2–2 Hz. Real ECU schedules remain subject to per-route safety budgets'),
        'stale_timeout_s': field('Stale timeout (s)', 3,'number','acquisition','runtime_restart',min=1,max=30),
        'source_preference': field('Source preference','can','select','acquisition','runtime_restart',choices=['can','uds','auto']),
        'can_uds_fallback': field('CAN → UDS fallback', True,'toggle','acquisition','runtime_restart'),
    },
    'logging': {
        'enabled': field('Enable logging', True,'toggle','logging'),
        'auto_start': field('Auto start logging', False,'toggle','logging'),
        'start_on_ignition': field('Start on ignition', False,'toggle','ignition'),
        'stop_on_ignition_off': field('Stop on ignition off', False,'toggle','ignition'),
        'format': field('Log format','jsonl','select','logging',choices=['jsonl']),
        'rotation_minutes': field('Rotate after (minutes)', 60,'number','logging',min=1,max=60),
        'max_storage_mb': field('Maximum recording storage (MB)', 100,'number','logging',min=1,max=1024),
        'retention_days': field('Retention (days)', 30,'number','logging',min=1,max=365),
        'signal_scope': field('Signals to record','active','select','logging',choices=['active','selected']),
        'selected_signals': field('Selected canonical signals',[],'signals','logging'),
    },
    'power': {
        'mode': field('Power mode','manual_switch','select','power_management','reboot',choices=['manual_switch','display_off','sleep','deep_sleep','hardware_off']),
        'ignition_detection': field('Ignition detection','none','select','ignition','reboot',choices=['none','gpio','can']),
        'boot_on_ignition': field('Boot on ignition',False,'toggle','ignition','reboot'),
        'shutdown_delay_s': field('Shutdown delay (s)',30,'number','power_management',min=0,max=3600),
        'sleep_delay_s': field('Sleep delay (s)',300,'number','power_management',min=0,max=86400),
        'display_off_delay_s': field('Display off delay (s)',120,'number','power_management',min=0,max=86400),
        'wake_behaviour': field('Wake behaviour','button','select','power_management',choices=['button','touch','ignition']),
    },
    'advanced': {
        'debug_level': field('Telemetry / debug level','info','select','debug',choices=['error','info','debug']),
        'event_verbosity': field('Event / log verbosity','normal','select','debug',choices=['minimal','normal','verbose']),
        'experimental_features': field('Experimental features',False,'toggle','experimental'),
    },
}

def settings_defaults(device_id='0000', startup='engine'):
    result={section:{key:deepcopy(spec['default']) for key,spec in fields.items()} for section,fields in FIELDS.items()}
    result['wifi']['ap_ssid']='MiniDash-'+device_id[-4:].upper()
    result['general']['startup_screen']=startup
    return result

def reject_secrets(value):
    if isinstance(value,dict):
        for key,item in value.items():
            if re.search(r'password|secret|credential|token',str(key),re.I):
                raise ValueError('Secrets are not allowed in configuration; use write-only secrets input')
            reject_secrets(item)
    elif isinstance(value,list):
        for item in value:reject_secrets(item)

def migrate_configuration(config, device_id='0000'):
    result=deepcopy(config)
    reject_secrets(result)
    version=result.get('schema_version')
    if type(version) is not int or version not in (1,2):
        raise ValueError('Unsupported configuration version; expected schema_version 1 or 2')
    if version==1:
        result['schema_version']=2
        result['device_settings']=settings_defaults(device_id,result.get('active_screen','engine'))
        result['device_settings']['vehicle']['profile']=result.get('settings',{}).get('vehicle_profile','')
    return result

def validate_settings(config):
    settings=config['device_settings']
    if not isinstance(settings,dict) or set(settings)!=set(FIELDS):raise ValueError('Invalid device settings sections')
    for section,fields in FIELDS.items():
        if not isinstance(settings[section],dict) or set(settings[section])!=set(fields):raise ValueError('Invalid settings fields in '+section)
        for key,spec in fields.items():
            value=settings[section][key];kind=spec['kind'];label=spec['label']
            if kind=='toggle' and type(value) is not bool:raise ValueError(label+' must be boolean')
            if kind in ('number','range'):
                if type(value) not in (int,float) or not math.isfinite(value) or not spec['min']<=value<=spec['max']:raise ValueError(label+' outside safe range')
                if 'step' not in spec and type(value) is not int:raise ValueError(label+' must be integer')
            if kind=='select' and (value not in spec['choices'] or isinstance(value,bool)):raise ValueError('Invalid '+label)
            if kind in ('text','ip'):
                if not isinstance(value,str) or len(value.encode('utf-8'))>spec.get('max_length',64) or '\x00' in value or (not value.strip() and not spec.get('allow_empty')):raise ValueError('Invalid '+label)
                if kind=='ip':
                    try:ipaddress.IPv4Address(value)
                    except ipaddress.AddressValueError:raise ValueError('Invalid '+label) from None
            if kind=='screen' and value not in [s['id'] for s in config['screens']]:raise ValueError('Startup screen not found')
            if kind=='signals':
                if not isinstance(value,list) or len(value)>128 or any(not isinstance(v,str) or not re.fullmatch(r'[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+',v) for v in value):raise ValueError('Invalid selected signals')
                if len(set(value))!=len(value):raise ValueError('Duplicate selected signals')
    g=settings['general'];w=settings['wifi'];a=settings['acquisition'];p=settings['power'];l=settings['logging']
    # Use the host's IANA database where installed; fixed UTC remains portable.
    try:ZoneInfo(g['timezone'])
    except ZoneInfoNotFoundError:
        if g['timezone'] not in ('UTC','Europe/Minsk','Europe/London','Europe/Berlin','America/New_York','Asia/Tokyo'):raise ValueError('Unknown timezone') from None
    expected={'ap':(True,False),'station':(False,True),'ap_station':(True,True)}[w['mode']]
    if (w['ap_enabled'],w['station_enabled'])!=expected:raise ValueError('Wi-Fi mode and enabled interfaces disagree')
    if w['station_enabled'] and not w['station_ssid'].strip():raise ValueError('Station SSID required')
    if w['station_address_mode']=='static':
        try:network=ipaddress.IPv4Network(w['station_ip']+'/'+w['station_netmask'],strict=False)
        except ValueError:raise ValueError('Invalid static network mask') from None
        if ipaddress.IPv4Address(w['station_gateway']) not in network:raise ValueError('Gateway must be on the static IP subnet')
        if ipaddress.IPv4Address(w['station_ip']) in (network.network_address,network.broadcast_address):raise ValueError('Static IP must be a host address')
    if not settings['vehicle']['read_only']:raise ValueError('SAFE / READ ONLY is mandatory; write/coding is not supported')
    if a['stale_timeout_s']<2/a['default_rate_hz']:raise ValueError('Stale timeout must cover at least two default update periods')
    if l['signal_scope']=='selected' and not l['selected_signals']:raise ValueError('Select at least one logging signal')
    if p['mode']=='hardware_off':raise ValueError('MOSFET hardware power cutoff is not implemented')
    return settings

def capabilities(mode, secrets_available=True):
    simulation=mode=='simulation'
    host={'general','logging','ui_refresh','diagnostics','settings_reset','factory_reset','debug'}
    simulated={'wifi_config','wifi_station','wifi_scan','display_brightness','display_rotation','can','uds','discovery','acquisition','power_management','restart_device','restart_wifi'}
    unavailable={'auto_brightness','display_fps','ignition','hardware_metrics','hardware_power_off','ota','experimental','read_only','clock_sync'}
    features={key:dict(supported=key in host or simulation and key in simulated,
                         scope='host' if key in host else 'simulation' if simulation and key in simulated else 'unavailable',
                         reason='' if key in host or simulation and key in simulated else 'Not supported by current firmware')
              for key in host|simulated|unavailable}
    if not secrets_available:
        for key in ('wifi_config','wifi_station'):
            features[key]=dict(supported=False,scope='unavailable',reason='Secure credential storage unavailable')
    return dict(schema_version=1,mode=mode,features=features,
                wifi_modes=['ap','station','ap_station'] if features['wifi_config']['supported'] else [],
                read_only=True,power=dict(master_switch='KCD11',hardware_cutoff=False,
                    note='Display off, sleep and deep sleep retain electrical consumption. Hardware off requires the master switch or a future MOSFET circuit.'))

def change_plan(previous,current,caps,secrets=None):
    changes=[]
    for section,fields in FIELDS.items():
        for key,spec in fields.items():
            before=previous['device_settings'][section][key];after=current['device_settings'][section][key]
            if before==after:continue
            feature=caps['features'][spec['capability']]
            if not feature['supported']:raise ValueError(spec['label']+': '+feature['reason'])
            if after=='ignition' and not caps['features']['ignition']['supported']:raise ValueError('Ignition wake is not supported')
            changes.append(dict(path=section+'.'+key,label=spec['label'],impact=spec['impact'],scope=feature['scope']))
    for key in secrets or {}:
        if key not in ('ap_password','station_password'):raise ValueError('Unknown secret field')
        capability='wifi_config' if key=='ap_password' else 'wifi_station'
        if not caps['features'][capability]['supported']:raise ValueError('Wi-Fi credential configuration unavailable')
        changes.append(dict(path='wifi.'+key,label='AP password' if key=='ap_password' else 'Station password',impact='wifi_reconnect',scope=caps['features'][capability]['scope']))
    impacts=sorted(set(row['impact'] for row in changes))
    wifi=current['device_settings']['wifi']
    return dict(changes=changes,impacts=impacts,reconnect_required='wifi_reconnect' in impacts,
                reboot_required='reboot' in impacts,runtime_restart_required='runtime_restart' in impacts,
                destination=dict(ssid=wifi['ap_ssid'] if wifi['ap_enabled'] else wifi['station_ssid'],
                    address='192.168.4.1' if wifi['ap_enabled'] else wifi['station_ip'] if wifi['station_address_mode']=='static' else 'Assigned by DHCP'),
                simulation=caps['mode']=='simulation')

def validate_credentials(settings, existing, changes):
    result=deepcopy(existing)
    for key,value in changes.items():
        if key not in ('ap_password','station_password') or not isinstance(value,str):raise ValueError('Invalid secret input')
        if value and not (8<=len(value)<=63 and all(32<=ord(c)<=126 for c in value) or len(value)==64 and re.fullmatch(r'[0-9a-fA-F]{64}',value)):
            raise ValueError('Wi-Fi password must be 8–63 printable ASCII characters or 64 hex digits')
        if value:result[key]=value
        else:result.pop(key,None)
    wifi=settings['wifi']
    if wifi['ap_enabled'] and wifi['ap_security']=='wpa2' and not result.get('ap_password'):
        raise ValueError('Set an AP password before applying WPA2 Wi-Fi configuration')
    if wifi['station_enabled'] and wifi['station_security']=='wpa2' and not result.get('station_password'):
        raise ValueError('Set a station password before applying secured station Wi-Fi configuration')
    return result

def sanitize(value, secrets=()):
    if isinstance(value,dict):
        return {key:sanitize(item,secrets) for key,item in value.items() if not re.search(r'password|secret|credential|token',str(key),re.I)}
    if isinstance(value,list):return [sanitize(item,secrets) for item in value]
    if isinstance(value,str):
        for secret in secrets:
            if secret:value=value.replace(secret,'[REDACTED]')
    return value

class WindowsSecretStore:
    available=os.name=='nt'
    @staticmethod
    def _crypt(data, decrypt=False):
        if os.name!='nt':raise ValueError('Secure credential storage unavailable')
        class Blob(ctypes.Structure):
            _fields_=[('size',ctypes.c_ulong),('data',ctypes.POINTER(ctypes.c_ubyte))]
        buffer=ctypes.create_string_buffer(data)
        incoming=Blob(len(data),ctypes.cast(buffer,ctypes.POINTER(ctypes.c_ubyte)));outgoing=Blob()
        library=ctypes.WinDLL('crypt32',use_last_error=True)
        function=library.CryptUnprotectData if decrypt else library.CryptProtectData
        function.argtypes=[ctypes.POINTER(Blob),ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ulong,ctypes.POINTER(Blob)]
        function.restype=ctypes.c_int
        if not function(ctypes.byref(incoming),None,None,None,None,1,ctypes.byref(outgoing)):
            raise OSError('Credential protection failed')
        try:return ctypes.string_at(outgoing.data,outgoing.size)
        finally:
            free=ctypes.WinDLL('kernel32').LocalFree;free.argtypes=[ctypes.c_void_p];free.restype=ctypes.c_void_p
            free(outgoing.data)
    @classmethod
    def seal(cls,secrets):
        if not secrets:return None
        return base64.b64encode(cls._crypt(json.dumps(secrets).encode())).decode('ascii')
    @classmethod
    def unseal(cls,protected):
        return json.loads(cls._crypt(base64.b64decode(protected),True)) if protected else {}
