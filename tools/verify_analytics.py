"""Offline canonical replay: independent Python arithmetic oracle + embedded C.

No CAN/serial/VCI imports or I/O. Synthetic counter encodings and thresholds
are fixtures, never production defaults or vehicle evidence.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
VALID, STALE, UNAVAILABLE, UNSUPPORTED, ERROR = range(5)
RANK = {VALID: 0, STALE: 1, UNAVAILABLE: 2, UNSUPPORTED: 3, ERROR: 4}


class Reference:
    """Deliberately smaller oracle; no firmware code/implementation imported."""
    def __init__(self):
        self.raw, self.extrema, self.counters = {}, {}, {}
        self.trip_misfires = self.session_misfires = self.last_delta = 0
        self.cylinder_totals = [0] * 4
        self.speed_previous = self.start = None
        self.performance = None

    def sample(self, name, value, state, stamp, ttl):
        if state == VALID and (not math.isfinite(value) or
                              ('ignition_retard_cyl' in name and value > 0) or
                              ('misfire_counter_cyl' in name and
                               (value < 0 or value > 0xffffffff or value != int(value)))):
            state = ERROR
        self.raw[name] = value, state, stamp, ttl

    def snapshot(self, name, now):
        value, state, stamp, ttl = self.raw.get(name, (0, UNAVAILABLE, 0, 0))
        if state == VALID and now - stamp >= ttl:
            state = STALE
        return state, value

    def tick(self, now):
        out = {name: self.snapshot(name, now) for name in self.raw}

        def formula(name, deps, fn):
            inputs = [out.get(d, (UNAVAILABLE, 0)) for d in deps]
            state = max((s for s, _ in inputs), key=RANK.get)
            out[name] = state, fn([v for _, v in inputs]) if state == VALID else 0

        formula('engine.boost', ['engine.manifold_absolute_pressure', 'engine.ambient_pressure'], lambda v: v[0]-v[1])
        formula('engine.charge_pressure_error', ['engine.charge_pressure_actual', 'engine.charge_pressure_requested'], lambda v: v[0]-v[1])
        formula('engine.charge_pressure_abs_error', ['engine.charge_pressure_error'], lambda v: abs(v[0]))
        formula('engine.fuel_trim_sum_b1', ['engine.fuel_trim_short_b1', 'engine.fuel_trim_long_b1'], sum)
        formula('engine.iat_ambient_delta', ['engine.intake_air_temperature', 'vehicle.ambient_air_temperature'], lambda v: v[0]-v[1])
        retard = [f'engine.ignition_retard_cyl{i}' for i in range(1, 5)]
        formula('engine.retard_worst', retard, min)
        formula('engine.retard_average', retard, lambda v: sum(v)/4)
        formula('engine.retard_worst_cylinder', retard, lambda v: v.index(min(v))+1 if min(v)<0 else 0)
        misfire = [f'engine.misfire_counter_cyl{i}' for i in range(1, 5)]
        formula('engine.misfire_total', misfire, sum)
        delta, changed = 0, False
        for i, name in enumerate(misfire):
            state, value = out.get(name, (UNAVAILABLE, 0))
            if state != VALID or value >= 65536:
                self.counters.pop(name, None)
                continue
            stamp = self.raw[name][2]
            previous = self.counters.get(name)
            if previous and stamp == previous[1]:
                continue
            changed = True
            d = 0
            if previous and 0 < stamp-previous[1] <= 5000:
                if value >= previous[0]:
                    d = value-previous[0]
                elif previous[0] >= 49152 and value < 16384:
                    d = 65536-previous[0]+value
            delta += d
            self.cylinder_totals[i] += d
            self.counters[name] = value, stamp
        if changed:
            self.last_delta = delta
        self.trip_misfires += delta
        self.session_misfires += delta
        for name, value in [('engine.misfire_delta', self.last_delta), ('trip.misfires', self.trip_misfires), ('session.misfires', self.session_misfires), ('engine.misfire_worst_cylinder', self.cylinder_totals.index(max(self.cylinder_totals))+1 if max(self.cylinder_totals)>0 else 0)]:
            out[name] = out['engine.misfire_total'][0], value
        peaks = {'trip.boost_peak': ('engine.boost', max), 'trip.retard_worst': ('engine.retard_worst', min), 'session.retard_worst': ('engine.retard_worst', min), 'trip.ecu_voltage_min': ('engine.control_module_voltage', min), 'trip.ecu_voltage_max': ('engine.control_module_voltage', max), 'trip.coolant_peak': ('engine.coolant_temperature', max), 'trip.oil_peak': ('engine.oil_temperature', max), 'trip.iat_peak': ('engine.intake_air_temperature', max), 'trip.dsg_oil_peak': ('transmission.oil_temperature', max), 'trip.haldex_clutch_peak': ('awd.clutch_temperature', max), 'trip.rpm_max': ('engine.rpm', max), 'trip.speed_max': ('vehicle.speed', max)}
        for name, (source, fn) in peaks.items():
            state, value = out.get(source, (UNAVAILABLE, 0))
            if state == VALID:
                self.extrema[name] = fn(self.extrema.get(name, value), value)
            out[name] = state, self.extrema.get(name, 0)
        state, speed = out.get('vehicle.speed', (UNAVAILABLE, 0))
        if state != VALID:
            self.speed_previous = self.start = self.performance = None
        else:
            stamp = self.raw['vehicle.speed'][2]
            if self.speed_previous and stamp != self.speed_previous[1]:
                old, old_stamp = self.speed_previous
                if stamp-old_stamp > 1000 or speed < old-1:
                    self.start = self.performance = None
                elif self.start is None and old == 0 and speed > 0:
                    self.start = old_stamp
                if self.start is not None and old < 100 <= speed:
                    self.performance = (old_stamp+(100-old)/(speed-old)*(stamp-old_stamp)-self.start)/1000
            self.speed_previous = speed, stamp
        out['performance.accel_0_100'] = (VALID, self.performance) if self.performance is not None else (state if state != VALID else UNAVAILABLE, 0)
        return out

    def reset(self, session):
        self.trip_misfires = 0
        self.cylinder_totals = [0]*4
        self.extrema = {k:v for k,v in self.extrema.items() if k.startswith('session.') and not session}
        self.performance = self.start = self.speed_previous = None
        if session:
            self.session_misfires = 0
            self.counters.clear()


def verify(harness, fixture=None):
    cases = json.loads((fixture or ROOT/'data/analytics/traces.json').read_text())['cases']
    comparisons = assertions = 0
    for case in cases:
        oracle = Reference()
        commands, reference = [], {}
        for row in case['steps']:
            now = row['ms']
            if 'reset' in row:
                commands.append(f"R {now} {row['reset']}")
                oracle.reset(row['reset']=='session')
            for name, sample in row.get('samples', {}).items():
                if isinstance(sample, (int, float)):
                    sample = {'value': sample}
                value, state, ttl = sample.get('value', 0), sample.get('state', VALID), sample.get('ttl', 5000)
                commands.append(f'S {now} {ttl} {state} {name} {value}')
                oracle.sample(name, value, state, now, ttl)
            commands.append(f'T {now}')
            reference[now] = oracle.tick(now)
        result = subprocess.run([str(harness), '--synthetic'], input='\n'.join(commands)+'\n', text=True, capture_output=True, check=True)
        actual = {}
        for line in result.stdout.splitlines():
            fields = line.split()
            if fields[0] == 'S':
                _, stamp, name, state, value, available = fields
                actual[int(stamp), name] = int(state), float(value), int(available)
        for row in case['steps']:
            for name, expected in row.get('expect', {}).items():
                assertions += 1
                state, value, available = actual[row['ms'], name]
                want_state = expected.get('state', VALID)
                assert state == want_state and available == int(want_state == VALID), (case['name'], row['ms'], name, state, want_state)
                if want_state == VALID:
                    assert math.isclose(value, expected['value'], rel_tol=1e-8, abs_tol=1e-6), (case['name'], name, value, expected)
                if expected.get('oracle', True):
                    comparisons += 1
                    ref_state, ref_value = reference[row['ms']][name]
                    assert ref_state == state, (case['name'], name, 'oracle state', ref_state, state)
                    if state == VALID:
                        assert math.isclose(ref_value, value, rel_tol=1e-8, abs_tol=1e-6), (case['name'], name, 'oracle', ref_value, value)
        print(case['name']+': PASS')
    for malformed in ['S -1 1 0 engine.rpm 1\n', 'S 0 0 0 engine.rpm 1\n', 'S 0 100 9 engine.rpm 1\n', 'S 0 100 0 trip.speed_max 1\n', 'S 0 100 0 unknown 1\n', 'T 0 trailing\n', 'S 0 100 0 engine.rpm 1x\n', 'X 0\n', 'T '+'9'*300+'\n']:
        result = subprocess.run([str(harness)], input=malformed, text=True, capture_output=True)
        assert result.returncode == 2, malformed
    return dict(status='PASS', scenarios=len(cases), embedded_assertions=assertions, oracle_comparisons=comparisons, malformed_inputs=9, vehicle_evidence=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--harness', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    summary = verify(args.harness.resolve())
    if args.output:
        args.output.write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
