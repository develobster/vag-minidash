"""Read-only driver inventory and conservative ECU identity discovery."""
from dataclasses import asdict, dataclass, fields
import os
import platform
import struct
from pathlib import Path

from .identity import EcuIdentity, IdentityContext, VehicleIdentity
from .uds import NegativeResponse, ReadClient


@dataclass(frozen=True)
class IdentityProbe:
    """One explicitly evidenced identification read.

    Only VIN/F190 is supplied as a generic default. Other VAG identification
    layouts remain data/config until their raw representation is captured.
    """
    did: int
    target: str
    encoding: str = "ascii"

    def __post_init__(self):
        vehicle = {item.name for item in fields(VehicleIdentity)}
        ecu = {item.name for item in fields(EcuIdentity)}
        group, separator, name = self.target.partition(".")
        if type(self.did) is not int or not 0 <= self.did <= 0xffff:
            raise ValueError("Identity probe DID must be a 16-bit integer")
        if not separator or group not in ("vehicle", "ecu") or \
                name not in (vehicle if group == "vehicle" else ecu):
            raise ValueError(f"Unknown identity target: {self.target}")
        if self.encoding not in ("ascii", "hex"):
            raise ValueError("Identity encoding must be ascii or hex")

    @classmethod
    def from_mapping(cls, value):
        did = value.get("did")
        if isinstance(did, str):
            did = int(did.removeprefix("0x").removeprefix("0X"), 16)
        return cls(did, value.get("target", ""), value.get("encoding", "ascii"))

    def decode(self, payload):
        if self.encoding == "hex":
            return payload.hex().upper()
        try:
            value = payload.rstrip(b"\x00\xff ").decode("ascii")
        except UnicodeDecodeError as exc:
            raise ValueError("Identity payload is not ASCII") from exc
        if not value or any(ord(char) < 0x20 or ord(char) > 0x7e for char in value):
            raise ValueError("Identity payload contains no printable ASCII value")
        return value


DEFAULT_IDENTITY_PROBES = (IdentityProbe(0xF190, "vehicle.vin"),)


@dataclass(frozen=True)
class DiscoveredEcu:
    role: str
    identity: EcuIdentity
    fingerprint: str
    fields: tuple
    errors: tuple


@dataclass(frozen=True)
class DiscoverySnapshot:
    vehicle: VehicleIdentity
    ecus: tuple
    unavailable: tuple = ()

    def contexts(self):
        return {item.role: IdentityContext(self.vehicle, item.identity) for item in self.ecus}

    def as_dict(self):
        return {"schema_version": 1, "vehicle": asdict(self.vehicle),
                "ecus": [asdict(item) for item in self.ecus],
                "unavailable": list(self.unavailable)}

    @classmethod
    def from_mapping(cls, value):
        if value.get("schema_version") != 1:
            raise ValueError("Unsupported discovery snapshot schema")
        ecus = []
        for item in value.get("ecus", ()):
            identity = EcuIdentity(**item["identity"])
            ecus.append(DiscoveredEcu(item["role"], identity, item["fingerprint"],
                                      tuple(item.get("fields", ())),
                                      tuple(item.get("errors", ()))))
        return cls(VehicleIdentity(**value.get("vehicle", {})), tuple(ecus),
                   tuple(value.get("unavailable", ())))


def discover_endpoint(role, transport_factory, *, vehicle=None, identity=None,
                      probes=DEFAULT_IDENTITY_PROBES, client_options=None, trace=lambda event: None):
    """Read an endpoint's evidenced identity fields; NRC never invents a value."""
    vehicle_values = {item.name: getattr(vehicle or VehicleIdentity(), item.name)
                      for item in fields(VehicleIdentity) if getattr(vehicle or VehicleIdentity(), item.name)}
    ecu_seed = identity or EcuIdentity(logical_role=role)
    expected = {item.name: getattr(ecu_seed, item.name) for item in fields(EcuIdentity)
                if getattr(ecu_seed, item.name)}
    # Address/role identify the endpoint. Knowledge selectors only become
    # observed identity after a corresponding successful probe.
    endpoint_fields = {"logical_role", "diagnostic_address", "request_id", "response_id"}
    ecu_values = {name: value for name, value in expected.items() if name in endpoint_fields}
    observed, errors = [], []
    transport = transport_factory()
    try:
        client = ReadClient(transport, trace=trace, **dict(client_options or {}))
        for probe in probes:
            try:
                value = probe.decode(client.read_did(probe.did))
                group, name = probe.target.split(".", 1)
                target = vehicle_values if group == "vehicle" else ecu_values
                expected_value = (vehicle_values.get(name) if group == "vehicle" else expected.get(name))
                if expected_value is not None and expected_value != value.upper():
                    raise ValueError(f"Observed {probe.target} conflicts with configured identity")
                target[name] = value
                observed.append({"did": f"{probe.did:04X}", "target": probe.target,
                                 "value": value})
            except NegativeResponse as exc:
                errors.append({"did": f"{probe.did:04X}", "target": probe.target,
                               "nrc": f"{exc.code:02X}"})
            except ValueError as exc:
                errors.append({"did": f"{probe.did:04X}", "target": probe.target,
                               "error": str(exc)})
    finally:
        transport.close()
    ecu = EcuIdentity(**ecu_values)
    return VehicleIdentity(**vehicle_values), DiscoveredEcu(
        role, ecu, ecu.fingerprint(), tuple(observed), tuple(errors))


def verify_discovered_identity(client, discovered, probes):
    """Re-read ECU identity on reconnect; configured/vehicle-only fields are not proof."""
    expected_targets = {item["target"]: item["value"] for item in discovered.fields
                        if item["target"].startswith("ecu.")}
    if not expected_targets:
        return False
    probe_by_target = {item.target: item for item in probes}
    for target, expected in expected_targets.items():
        probe = probe_by_target.get(target)
        if probe is None:
            return False
        try:
            observed = probe.decode(client.read_did(probe.did))
        except (NegativeResponse, ValueError):
            return False
        if observed.upper() != str(expected).upper():
            return False
    return True


def discover_vehicle(vehicle, endpoints):
    """Discover configured physical endpoints without scanning or broadcasts."""
    current = vehicle or VehicleIdentity()
    discovered, unavailable = [], []
    for endpoint in endpoints:
        try:
            observed_vehicle, ecu = discover_endpoint(
                endpoint["role"], endpoint["factory"], vehicle=current,
                identity=endpoint.get("identity"), probes=endpoint.get("probes", DEFAULT_IDENTITY_PROBES),
                client_options=endpoint.get("client_options"), trace=endpoint.get("trace", lambda event: None))
        except Exception as exc:
            unavailable.append({"role": endpoint["role"], "error": str(exc),
                                "type": type(exc).__name__})
            continue
        current_values = {item.name: getattr(current, item.name) for item in fields(VehicleIdentity)}
        for item in fields(VehicleIdentity):
            value = getattr(observed_vehicle, item.name)
            if value is not None:
                if current_values[item.name] is not None and current_values[item.name] != value:
                    raise ValueError(f"Conflicting vehicle identity field: {item.name}")
                current_values[item.name] = value
        current = VehicleIdentity(**current_values)
        discovered.append(ecu)
    return DiscoverySnapshot(current, tuple(discovered), tuple(unavailable))


def pe_info(path):
    """Read PE machine type and exported symbol names without executing DLL code."""
    data = Path(path).read_bytes()
    def u16(offset): return struct.unpack_from('<H',data,offset)[0]
    def u32(offset): return struct.unpack_from('<I',data,offset)[0]
    if data[:2] != b'MZ':
        raise ValueError('Not a PE file')
    pe = u32(0x3c)
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    optional = pe + 24
    magic = u16(optional)
    if magic not in (0x10b,0x20b):
        raise ValueError('Unsupported PE optional header')
    sections = []
    section_table = optional + u16(pe+20)
    for n in range(u16(pe+6)):
        s = section_table + n*40
        sections.append((u32(s+12),max(u32(s+8),u32(s+16)),u32(s+20)))
    def offset(rva):
        for start,size,raw in sections:
            if start <= rva < start+size:
                result = raw+rva-start
                if result >= len(data):
                    raise ValueError('RVA outside file')
                return result
        raise ValueError('RVA outside sections')
    exports = []
    export_rva = u32(optional + (96 if magic == 0x10b else 112))
    if export_rva:
        table = offset(export_rva)
        count = u32(table+24)
        if count > 100000:
            raise ValueError('Unreasonable export count')
        names = offset(u32(table+32)) if count else 0
        for n in range(count):
            name = offset(u32(names+4*n))
            end = data.find(b'\0',name,min(name+512,len(data)))
            if end < 0:
                raise ValueError('Unterminated export name')
            exports.append(data[name:end].decode('ascii',errors='replace'))
    return {'path':str(path),'machine':hex(u16(pe+4)),
            'bits':32 if magic == 0x10b else 64,
            'diagnostic_exports':[s for s in exports if 'PassThru' in s or s.startswith('PDU') or s.startswith('_PDU')]}


def inventory(paths=()):
    result = {'platform':platform.system(),'python_bits':struct.calcsize('P')*8,
              'vehicle_io_performed':False,'j2534':[],'d_pdu_registry':[], 'dlls':[], 'errors':[]}
    candidates = set(paths)
    if os.name == 'nt':
        import winreg as w
        def values(key):
            out = {}
            for n in range(w.QueryInfoKey(key)[1]):
                name,value,_ = w.EnumValue(key,n)
                out[name] = value if isinstance(value,(str,int,list)) else repr(value)
            return out
        for bits,flag in ((32,w.KEY_WOW64_32KEY),(64,w.KEY_WOW64_64KEY)):
            for version in ('04.04','05.00'):
                base = 'SOFTWARE\\PassThruSupport.'+version
                try:
                    with w.OpenKey(w.HKEY_LOCAL_MACHINE,base,0,w.KEY_READ|flag) as parent:
                        for n in range(w.QueryInfoKey(parent)[0]):
                            name = w.EnumKey(parent,n)
                            with w.OpenKey(parent,name) as child:
                                entry = values(child)
                                result['j2534'].append({'registry_bits':bits,'api_version':version,'name':name,**entry})
                                if entry.get('FunctionLibrary'):
                                    candidates.add(os.path.expandvars(entry['FunctionLibrary']))
                except FileNotFoundError:
                    pass
                except OSError as exc:
                    result['errors'].append(f'{base} ({bits}): {exc}')
            for base in ('SOFTWARE\\D-PDU API','SOFTWARE\\D-PDUAPI'):
                try:
                    with w.OpenKey(w.HKEY_LOCAL_MACHINE,base,0,w.KEY_READ|flag) as key:
                        result['d_pdu_registry'].append({'path':base,'bits':bits,'values':values(key),
                            'subkeys':[w.EnumKey(key,n) for n in range(w.QueryInfoKey(key)[0])]})
                except FileNotFoundError:
                    pass
                except OSError as exc:
                    result['errors'].append(str(exc))
    for path in sorted(candidates):
        try:
            result['dlls'].append(pe_info(path))
        except (OSError,ValueError,struct.error) as exc:
            result['errors'].append(f'{path}: {exc}')
    result['interpretation'] = ('J2534 registration/exports are candidates, not proof of device connection. '
        'PDU exports indicate D-PDU API, not J2534. Wi-Fi/IP connectivity does not prove DoIP. '
        'An empty inventory does not prove absence: inspect ODIS loaded modules/root XML.')
    return result
