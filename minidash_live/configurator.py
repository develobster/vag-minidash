"""Host configurator contracts. Settings never change ECU acquisition or firmware.

Uses the existing canonical SignalStore for simulation; no CAN IDs or DIDs.
"""
from copy import deepcopy
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import threading
import time
import uuid
from .device_settings import migrate_configuration, validate_settings, settings_defaults
from .settings_service import SettingsService

SIGNALS = {
    "engine.rpm": ("Engine speed", "rpm", 0, 8000),
    "vehicle.speed": ("Speed", "km/h", 0, 260),
    "engine.coolant_temperature": ("Coolant", "°C", 0, 130),
    "engine.oil_temperature": ("Oil temperature", "°C", 0, 150),
    "engine.charge_pressure_actual": ("Charge pressure", "hPa", 800, 3000),
    "engine.ambient_pressure": ("Ambient pressure", "hPa", 800, 1200),
    "vehicle.battery_voltage": ("Battery", "V", 10, 16),
    "transmission.gear": ("Gear", "", 0, 7),
    "wheel.front_left.speed": ("Front left wheel", "km/h", 0, 260),
    "wheel.front_right.speed": ("Front right wheel", "km/h", 0, 260),
    "engine.boost_pressure": ("Boost (relative)", "bar", -1, 3),
}
PRESETS = {
    "OEM Dark": ("#0b1016", "#eff5fa", "#59dfc0"),
    "Sport": ("#140e13", "#fff0f3", "#ff5577"),
    "Minimal": ("#f1f4f5", "#17212b", "#007d68"),
    "Night": ("#090606", "#f7b780", "#e18b45"),
    "High Contrast": ("#000000", "#ffffff", "#ffff00"),
}

def widget(signal="engine.rpm", index=0):
    label, unit, low, high = SIGNALS.get(signal, (signal, "", 0, 100))
    return dict(id=uuid.uuid4().hex, signal=signal, label=label, units=unit,
                type="numeric", x=index % 2, y=index // 2, width=1, height=1,
                min=low, max=high, warning=high*.8, critical=high*.9, precision=0)

def defaults():
    themes = [dict(id=name.lower().replace(" ", "-"), name=name,
                   background=colors[0], foreground=colors[1], accent=colors[2],
                   warning="#ffbc55", critical="#ff596a", gauge="round",
                   typography="system", spacing=8, grid=False)
              for name, colors in PRESETS.items()]
    result = dict(schema_version=2, settings=dict(appearance="dark", vehicle_profile="",
                display_width=320, display_height=240), active_screen="engine",
                active_theme="oem-dark", themes=themes, signal_configuration={},
                screens=[dict(id="engine", name="Engine", columns=2, widgets=[
                    widget(name, i) for i, name in enumerate(("engine.rpm", "vehicle.speed",
                    "engine.coolant_temperature", "vehicle.battery_voltage"))])])
    result['device_settings']=settings_defaults()
    return result

def validate_config(config):
    """Reject unknown versions, malformed imports and unbounded display geometry."""
    if not isinstance(config,dict): raise ValueError('Configuration object required')
    config=migrate_configuration(config)
    def text(value, limit=80):
        if not isinstance(value, str) or not value or len(value) > limit:
            raise ValueError("Invalid text or identifier")
    def number(value, low, high):
        if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError("Invalid numeric setting")
    screens, themes = config.get("screens"), config.get("themes")
    if not isinstance(screens, list) or not 1 <= len(screens) <= 16:
        raise ValueError("Use 1–16 screens")
    if not isinstance(themes, list) or not 1 <= len(themes) <= 32:
        raise ValueError("Use 1–32 themes")
    for collection in (screens, themes):
        ids = []
        for row in collection:
            text(row["id"]); text(row["name"]); ids.append(row["id"])
        if len(set(ids)) != len(ids):
            raise ValueError("Duplicate identifiers")
    for screen in screens:
        if type(screen["columns"]) is not int or not 1 <= screen["columns"] <= 4:
            raise ValueError("Invalid column count")
        if not isinstance(screen["widgets"], list) or not 1 <= len(screen["widgets"]) <= 24:
            raise ValueError("Use 1–24 widgets per screen")
        occupied, ids = set(), set()
        for w in screen["widgets"]:
            text(w["id"]); text(w["signal"], 120); text(w["label"])
            if w["id"] in ids: raise ValueError("Duplicate widget")
            ids.add(w["id"])
            if not re.fullmatch(r"[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+", w["signal"]):
                raise ValueError("Canonical signal name required")
            if not isinstance(w["units"], str) or len(w["units"]) > 20:
                raise ValueError("Invalid units")
            if w["type"] not in ("numeric", "gauge", "bar", "compact", "graph"):
                raise ValueError("Invalid visualization")
            for key, low, high in (("x",0,3),("y",0,23),("width",1,4),("height",1,4),("precision",0,3)):
                if type(w[key]) is not int: raise ValueError("Integer geometry required")
                number(w[key], low, high)
            if w["x"] + w["width"] > screen["columns"] or w["y"] + w["height"] > 24:
                raise ValueError("Widget outside grid")
            for y in range(w["y"], w["y"]+w["height"]):
                for x in range(w["x"], w["x"]+w["width"]):
                    if (x,y) in occupied: raise ValueError("Widgets overlap")
                    occupied.add((x,y))
            for key in ("min", "max", "warning", "critical"): number(w[key], -1e9, 1e9)
            if not w["min"] < w["max"] or not w["min"] <= w["warning"] <= w["critical"] <= w["max"]:
                raise ValueError("Use min ≤ warning ≤ critical ≤ max")
    for theme in themes:
        for key in ("background", "foreground", "accent", "warning", "critical"):
            if not re.fullmatch(r"#[0-9a-fA-F]{6}", theme[key]): raise ValueError("Invalid theme color")
        if theme["gauge"] not in ("round", "square") or theme["typography"] not in ("system", "mono"):
            raise ValueError("Invalid theme style")
        number(theme["spacing"], 0, 24)
        if type(theme["grid"]) is not bool: raise ValueError("Invalid grid setting")
    if config.get("active_screen") not in [s["id"] for s in screens] or config.get("active_theme") not in [t["id"] for t in themes]:
        raise ValueError("Active screen/theme missing")
    settings = config["settings"]
    if settings["appearance"] not in ("auto", "light", "dark"): raise ValueError("Invalid appearance")
    if not isinstance(settings["vehicle_profile"], str) or len(settings["vehicle_profile"]) > 120:
        raise ValueError("Invalid vehicle preference")
    for key in ("display_width", "display_height"):
        if type(settings[key]) is not int: raise ValueError("Integer display dimensions required")
        number(settings[key], 128, 1024)
    if not isinstance(config.get("signal_configuration"), dict): raise ValueError("Invalid signal configuration")
    validate_settings(config)
    config['settings']['vehicle_profile']=config['device_settings']['vehicle']['profile']
    return deepcopy(config)

def atomic_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(data, ensure_ascii=False, allow_nan=False), encoding="utf-8")
    temp.replace(path)

class Configurator(SettingsService):
    def __init__(self, state, directory):
        self.state, self.directory = state, Path(directory)
        self.lock = threading.RLock()
        self.started = time.monotonic()
        path = self.directory / "configuration.json"
        saved = json.loads(path.read_text(encoding="utf-8")) if path.exists() else None
        initial_id=(saved or {}).get('device_id') or uuid.uuid5(uuid.NAMESPACE_URL,str(self.directory.resolve())).hex
        self.config = validate_config(migrate_configuration(saved.get("configuration", saved),initial_id)) if saved else defaults()
        self.revision = saved.get("revision", 0) if saved else 0
        self.active = None
        self.recording_error = None
        self._init_settings(saved)
        if not saved:
            self.config['device_settings']['wifi']['ap_ssid']='MiniDash-'+self.device_id[-4:].upper()
        # Catalog metadata is read-only; presence never enables CAN/UDS acquisition.
        from .vag import VagKnowledgeProvider
        self.catalog = {}
        for definition in VagKnowledgeProvider().definitions():
            name = definition.canonical_name
            if not name.startswith('raw.'):
                self.catalog.setdefault(name, dict(signal=name, label=name.replace('.', ' ').replace('_',' ').title(),
                    unit=definition.unit, min=None, max=None, value=None, quality='unavailable',
                    source='unavailable', age=None, expected_period=definition.expected_period))

    def configuration(self):
        with self.lock: return dict(configuration=deepcopy(self.config), revision=self.revision,secret_status=self.secret_status())

    def save(self, body):
        return self.apply_settings(body)

    def telemetry(self):
        original = json.loads(self.state.snapshot())
        signals = original.get("signals", {})
        rows = list(signals.values()) if isinstance(signals, dict) else signals
        catalog = {**deepcopy(self.catalog), **{name:dict(signal=name, label=meta[0], unit=meta[1], min=meta[2], max=meta[3],
                    value=None, quality="unavailable", source="unavailable", age=None, expected_period=None)
                   for name, meta in SIGNALS.items()}}
        for row in rows:
            name = row.get("canonical_name") or row.get("signal")
            if not name: continue
            age = row.get("age")
            if row.get("received_at") is not None:
                age = time.monotonic()-row["received_at"]+row.get("initial_age", 0)
            quality = row.get("quality", "unavailable")
            if age is None or age >= row.get("ttl", 3): quality = "stale"
            catalog[name] = {**catalog.get(name, dict(signal=name, label=name, min=None, max=None)),
                             **row, "signal":name, "age":age, "quality":quality,
                             "value":row.get("value") if quality == "valid" else None,
                             "source":row.get("source", "uds")}
        mode = getattr(self.state, "mode", "research")
        if mode != 'simulation':
            actual, ambient = catalog['engine.charge_pressure_actual'], catalog['engine.ambient_pressure']
            if actual['quality'] == ambient['quality'] == 'valid' and actual.get('unit') == ambient.get('unit') == 'hPa':
                catalog['engine.boost_pressure'].update(value=(actual['value']-ambient['value'])/1000,
                    quality='valid', source=actual['source'] if actual['source']==ambient['source'] else 'derived',
                    age=max(actual['age'],ambient['age']), derivation='(absolute charge pressure - ambient pressure) / 1000')
        with self.lock:
            recording = self._recording_status()
        return dict(mode=mode, status=original.get("status", "unknown"), signals=list(catalog.values()),
                    uptime=time.monotonic()-self.started, logging=recording,
                    device=dict(**self.device_info(),platform="Windows/Python host", firmware=None,
                        can="simulated" if mode == "simulation" else original.get("can_state", "unknown"),
                        vehicle="simulation" if mode == "simulation" else original.get("vehicle_state", "unknown"),
                        ecus=original.get("ecus", []), diagnostics=original.get("metrics", {})),
                    capabilities=dict(logging=True, configuration=True, ota=False, firmware_settings=False,
                                      physical_display_preview=False), device_capabilities=self.device_capabilities(),
                    operation=deepcopy(self.operation), active_screen=self.runtime_screen)

    def _recording_status(self):
        if not self.active: return dict(active=False, error=self.recording_error)
        return dict(active=True, id=self.active["id"], duration=time.monotonic()-self.active["started"],
                    samples=self.active["samples"], bytes=self.active["bytes"], signals=self.active["signals"])

    def start_recording(self):
        with self.lock:
            if self.active: raise RuntimeError("Recording already active")
            settings=self.config['device_settings']['logging']
            if not settings['enabled']:raise ValueError('Logging is disabled in Device settings')
            self._enforce_retention()
            self.recording_error = None
            signals = sorted({w["signal"] for s in self.config["screens"] for w in s["widgets"]})
            if settings['signal_scope']=='selected':signals=settings['selected_signals'][:]
            identifier = uuid.uuid4().hex
            path = self.directory / "logs" / (identifier+".jsonl")
            path.parent.mkdir(parents=True, exist_ok=True)
            self.active = dict(id=identifier, date=datetime.now(timezone.utc).isoformat(),
                mode=getattr(self.state, "mode", "research"), started=time.monotonic(), samples=0,
                bytes=0, signals=signals, file=path.open("a", encoding="utf-8"))
            return self._recording_status()

    def sample(self):
        telemetry = self.telemetry()
        with self.lock:
            if not self.active: return
            row = dict(time=datetime.now(timezone.utc).isoformat(), mode=telemetry["mode"],
                       signals=[r for r in telemetry["signals"] if r["signal"] in self.active["signals"]])
            line = json.dumps(row, ensure_ascii=False, allow_nan=False)+"\n"
            self.active["file"].write(line); self.active["file"].flush()
            self.active["samples"] += 1; self.active["bytes"] += len(line.encode())
            settings=self.config['device_settings']['logging']
            total=sum(p.stat().st_size for p in (self.directory/'logs').glob('*.jsonl'))
            if self.active["bytes"] >= 20_000_000 or time.monotonic()-self.active["started"] >= settings['rotation_minutes']*60 or total>=settings['max_storage_mb']*1_000_000:
                self.stop_recording()

    def _enforce_retention(self):
        settings=self.config['device_settings']['logging']
        rows=self.recordings()
        total=sum(p.stat().st_size for p in (self.directory/'logs').glob('*.jsonl'))
        for row in reversed(rows):
            expired=(datetime.now(timezone.utc)-datetime.fromisoformat(row['date'])).total_seconds()>settings['retention_days']*86400
            if expired or total>=settings['max_storage_mb']*1_000_000:
                path=self.log_path(row['id']);total-=path.stat().st_size
                path.unlink();path.with_suffix('.json').unlink()
        if total>=settings['max_storage_mb']*1_000_000:raise ValueError('Recording storage full; remove incomplete files or increase the storage limit')

    def stop_recording(self):
        with self.lock:
            if not self.active: raise RuntimeError("No active recording")
            meta = {k:v for k,v in self.active.items() if k not in ("file", "started")}
            meta["duration"] = time.monotonic()-self.active["started"]
            self.active["file"].close()
            self.active = None
            try:
                atomic_json(self.directory / "logs" / (meta["id"]+".json"), meta)
            except OSError:
                self.recording_error = 'Recording metadata could not be saved: storage unavailable'
                raise
            return meta

    def recordings(self):
        with self.lock:
            return [json.loads(p.read_text(encoding="utf-8")) for p in sorted(
                (self.directory / "logs").glob("*.json"), key=lambda p:p.stat().st_mtime, reverse=True)]

    def log_path(self, identifier):
        if not re.fullmatch(r"[a-f0-9]{32}", identifier): raise ValueError("Invalid recording id")
        path = self.directory / "logs" / (identifier+".jsonl")
        if not path.with_suffix(".json").exists(): raise FileNotFoundError("Recording not found or still active")
        return path

class SimulationState:
    mode = "simulation"
    def __init__(self):
        from .runtime import SignalStore
        self.store, self.started = SignalStore(), time.monotonic()
        self.paused = False

    def step(self):
        if self.paused: return
        if getattr(self,'device_settings',{}).get('power',{}).get('mode') in ('sleep','deep_sleep'):return
        t = time.monotonic()-self.started
        wave = (math.sin(t/8)+1)/2
        values = (800+wave*4400, wave*125, 88+math.sin(t/17)*5,
                  98+math.sin(t/21)*8, 995+wave*1200, 995+math.sin(t/30)*4, 14.2+math.sin(t/3)*.2,
                  min(7,max(1,int(wave*6)+1)), wave*125, wave*125+math.sin(t)*.3,
                  (wave*1200-math.sin(t/30)*4)/1000)
        for (name, meta), value in zip(SIGNALS.items(), values):
            acquisition=getattr(self,'device_settings',{}).get('acquisition',{})
            self.store.publish("synthetic", name, "simulated", value, acquisition.get('stale_timeout_s',3), unit=meta[1],
                               expected_period=1/acquisition.get('default_rate_hz',2), definition_id="simulation."+name)

    def snapshot(self):
        return json.dumps(dict(status="simulation_paused" if self.paused else "active",
                               signals=self.store.snapshot())).encode()

def main():
    import argparse
    from .api import serve, State
    parser = argparse.ArgumentParser(description="MiniDash host Web Configurator (canonical simulation, no vehicle I/O)")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--data-dir", default=".minidash")
    parser.add_argument('--host-only',action='store_true',help='Serve host settings without simulated telemetry or hardware capabilities')
    args = parser.parse_args()
    state = State() if args.host_only else SimulationState()
    if args.host_only:state.mode='host'
    else:state.step()
    server, thread = serve(state, args.port, host=args.host, data_dir=args.data_dir)
    print(f"{state.mode.upper()} - Web Configurator http://{args.host}:{server.server_port}", flush=True)
    try:
        while True:
            if not args.host_only:state.step()
            time.sleep(1/server.configurator.config['device_settings']['acquisition']['default_rate_hz'])
    except KeyboardInterrupt: pass
    finally: server.shutdown(); server.server_close(); thread.join(2)

if __name__ == "__main__": main()
