"""Read-only driver inventory; never loads a DLL or opens a vehicle channel."""
import os
import platform
import struct
from pathlib import Path


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
                if result >= len(data): raise ValueError('RVA outside file')
                return result
        raise ValueError('RVA outside sections')
    exports = []
    export_rva = u32(optional + (96 if magic == 0x10b else 112))
    if export_rva:
        table = offset(export_rva)
        count = u32(table+24)
        if count > 100000: raise ValueError('Unreasonable export count')
        names = offset(u32(table+32)) if count else 0
        for n in range(count):
            name = offset(u32(names+4*n))
            end = data.find(b'\0',name,min(name+512,len(data)))
            if end < 0: raise ValueError('Unterminated export name')
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
