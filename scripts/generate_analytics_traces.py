"""Original synthetic canonical traces, deliberately unrelated to ECU wire IDs."""
from pathlib import Path
import json
ROOT = Path(__file__).resolve().parents[1]
cases = []
def step(ms, samples=None, expect=None, **kwargs):
    return dict(ms=ms, samples=samples or {}, expect={k: v if isinstance(v, dict) else {'value': v} for k,v in (expect or {}).items()}, **kwargs)
def add(name, steps):
    cases.append(dict(name=name, steps=steps))
def cylinders(kind, values):
    return {f'engine.{kind}_cyl{i}': v for i,v in enumerate(values, 1)}
ret=lambda v: cylinders('ignition_retard', v)
mis=lambda v: cylinders('misfire_counter', v)
embedded=lambda v: dict(value=v, oracle=False)
state=lambda s: dict(state=s)
add('normal_drive', [step(0, {'engine.manifold_absolute_pressure':1500,'engine.ambient_pressure':1000,'engine.rpm':2200,'vehicle.speed':40,'engine.fuel_trim_short_b1':3,'engine.fuel_trim_long_b1':-1,'engine.control_module_voltage':14}, {'engine.boost':500,'engine.fuel_trim_sum_b1':2,'trip.rpm_max':2200,'trip.ecu_voltage_min':14}),step(1000, {'engine.manifold_absolute_pressure':1700,'engine.rpm':3000,'vehicle.speed':50}, {'engine.boost':700,'trip.boost_peak':700,'engine.boost_average_5s':embedded(600)})])
add('knock_burst', [step(0, ret([0,0,0,0])), step(100, ret([-1,-4,0,-2]), {'engine.retard_worst':-4,'engine.retard_average':-1.75,'engine.retard_worst_cylinder':2}),step(600,expect={'session.retard_events':embedded(1),'trip.retard_worst':-4}),step(1000,ret([0,0,0,0]),{'session.retard_events':embedded(1)})])
add('single_cylinder_repeated_retard', [step(0, ret([0,0,0,0])),step(100,ret([0,0,-3,0])),step(600,expect={'session.retard_events':embedded(1)}),step(2700,ret([0,0,0,0])),step(2800,ret([0,0,-5,0])),step(3300,expect={'session.retard_events':embedded(2),'engine.retard_worst_cylinder':3,'trip.retard_worst':-5})])
add('misfire_burst', [step(0,mis([100,100,100,100])),step(1000,mis([100,104,101,100]),{'engine.misfire_total':405,'engine.misfire_delta':5,'trip.misfires':5,'engine.misfires_per_min':embedded(300),'engine.misfire_worst_cylinder':2}),step(2000,mis([100,106,101,100]),{'trip.misfires':7,'engine.misfires_per_min':embedded(210)})])
add('counter_reset_and_overflow', [step(0,mis([65534,10,0,0])),step(1000,mis([2,11,0,0]),{'engine.misfire_delta':5,'trip.misfires':5}),step(2000,mis([0,0,0,0]),{'engine.misfire_delta':0,'trip.misfires':5}),step(3000,mis([1,0,0,0]),{'trip.misfires':6}),step(3100,mis([2,0,0,0]),{'trip.misfires':1,'session.misfires':7},reset='trip')])
add('overboost', [step(0,{'engine.charge_pressure_actual':2000,'engine.charge_pressure_requested':1700},{'engine.charge_pressure_error':300,'engine.charge_pressure_abs_error':300}),step(500,expect={'session.overboost_events':embedded(1)}),step(700,expect={'session.overboost_events':embedded(1)})])
add('underboost', [step(0,{'engine.charge_pressure_actual':1400,'engine.charge_pressure_requested':1800},{'engine.charge_pressure_error':-400,'engine.charge_pressure_abs_error':400}),step(500,expect={'session.underboost_events':embedded(1)})])
add('heat_soak_and_warmup', [step(0,{'engine.intake_air_temperature':50,'vehicle.ambient_air_temperature':20,'vehicle.speed':0,'engine.rpm':800,'engine.coolant_temperature':60},{'engine.iat_ambient_delta':30,'engine.heat_soak_proxy':embedded(1),'trip.coolant_warmup_time':dict(state=2,oracle=False)}),step(1000,{'engine.coolant_temperature':80,'engine.intake_air_temperature':55},{'trip.coolant_warmup_time':embedded(1),'trip.iat_peak':55,'engine.coolant_rise_rate':embedded(20)}),step(2000,{'engine.coolant_temperature':105}, {'trip.coolant_peak':105}),step(2500,expect={'trip.coolant_high_seconds':embedded(.5)})])
add('voltage_sag', [step(0,{'engine.control_module_voltage':14}),step(100,{'engine.control_module_voltage':10}),step(600,expect={'session.ecu_voltage_drop_events':embedded(1),'trip.ecu_voltage_min':10,'trip.ecu_voltage_low_seconds':embedded(.5)}),step(1100,{'engine.control_module_voltage':14},{'trip.ecu_voltage_min':10,'trip.ecu_voltage_max':14})])
add('signal_loss_reconnect', [step(0,{'engine.manifold_absolute_pressure':1600,'engine.ambient_pressure':1000},{'engine.boost':600}),step(100,{'engine.ambient_pressure':{'state':2}}, {'engine.boost':state(2),'trip.boost_peak':state(2),'session.signal_loss_events':embedded(1)}),step(200,{'engine.ambient_pressure':1000,'engine.manifold_absolute_pressure':1700},{'engine.boost':700,'trip.boost_peak':700})])
add('stale_dependency_and_unsupported', [step(0,{'engine.manifold_absolute_pressure':1600,'engine.ambient_pressure':{'value':1000,'ttl':100}}, {'engine.boost':600}),step(100,expect={'engine.boost':state(1),'trip.boost_peak':state(1)}),step(200,{'engine.ambient_pressure':{'state':3}}, {'engine.boost':state(3)}),step(300,{'engine.ambient_pressure':1000,'engine.manifold_absolute_pressure':1800},{'trip.boost_peak':800})])
steps=[step(ms,{'vehicle.speed':ms/100}, {'performance.accel_0_100':10,'trip.speed_max':100,'trip.performance_runs':embedded(4)} if ms==10000 else None) for ms in range(0,10001,1000)]
steps.append(step(16000,expect={'performance.accel_0_100':state(1)}))
add('acceleration_run_then_loss',steps)
out=ROOT/'data/analytics/traces.json'
out.write_text(json.dumps(dict(schema=1,synthetic=True,configuration='analytics-harness --synthetic; test-only thresholds/16-bit counters',cases=cases),indent=2)+'\n')
print(f'{len(cases)} synthetic scenarios')
