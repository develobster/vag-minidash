"""Generate the append-only embedded analytics vocabulary; no vehicle addresses."""
from pathlib import Path
import json,re
ROOT = Path(__file__).resolve().parents[1]
# Existing IDs 0..13 must never move. New raw IDs fit the acquisition mask.
raw = [
("IAT","engine.intake_air_temperature","IAT","degC","TEMPERATURE",0),
("AIR_TEMP","vehicle.ambient_air_temperature","AMBIENT AIR","degC","TEMPERATURE",0),
("STFT","engine.fuel_trim_short_b1","STFT B1","%","PERCENT",1),
("LTFT","engine.fuel_trim_long_b1","LTFT B1","%","PERCENT",1),
*[(f"RETARD_{i}",f"engine.ignition_retard_cyl{i}",f"RETARD C{i}","deg","NONE",1) for i in range(1,5)],
*[(f"MISFIRE_{i}",f"engine.misfire_counter_cyl{i}",f"MISFIRE C{i}","count","NONE",0) for i in range(1,5)],
("CHARGE_REQUESTED","engine.charge_pressure_requested","CHARGE REQ","hPa","PRESSURE",0)]
# op, dependencies, parameter. Peak/min statistics are observed trip/session values.
derived=[]
def add(key,name,label,unit,family,decimals,op,deps,param=0):
 derived.append((key,name,label,unit,family,decimals,op,deps,param))
add("RETARD_WORST","engine.retard_worst","WORST RETARD","deg","NONE",1,"MIN4",["RETARD_1","RETARD_2","RETARD_3","RETARD_4"])
add("RETARD_AVG","engine.retard_average","AVG RETARD","deg","NONE",1,"AVG4",["RETARD_1","RETARD_2","RETARD_3","RETARD_4"])
add("RETARD_CYL","engine.retard_worst_cylinder","RETARD CYL","cyl","NONE",0,"WORST4",["RETARD_1","RETARD_2","RETARD_3","RETARD_4"])
add("KNOCK_COUNT","session.retard_events","RETARD EVENTS","count","NONE",0,"KNOCK_COUNT",["RETARD_WORST"])
add("KNOCK_RATE","engine.retard_events_per_min","RETARD /MIN","1/min","NONE",1,"KNOCK_RATE",["RETARD_WORST"])
add("RETARD_LOAD_PEAK","engine.retard_load_event_peak","LOAD RETARD","deg","NONE",1,"LOAD_PEAK",["RETARD_WORST","ENGINE_LOAD"])
add("RETARD_TRIP_PEAK","trip.retard_worst","TRIP RETARD","deg","NONE",1,"TRIP_MIN",["RETARD_WORST"])
add("RETARD_SESSION_PEAK","session.retard_worst","SESS RETARD","deg","NONE",1,"SESSION_MIN",["RETARD_WORST"])
for k,n,l,op in [("MISFIRE_TOTAL","engine.misfire_total","MISFIRE SUM","SUM4"),("MISFIRE_DELTA","engine.misfire_delta","MISFIRE DELTA","MISFIRE_DELTA"),("MISFIRE_RATE","engine.misfires_per_min","MISFIRE /MIN","MISFIRE_RATE"),("MISFIRE_TRIP","trip.misfires","TRIP MISFIRE","MISFIRE_TRIP"),("MISFIRE_SESSION","session.misfires","SESS MISFIRE","MISFIRE_SESSION"),("MISFIRE_CYL","engine.misfire_worst_cylinder","MISFIRE CYL","MISFIRE_CYL")]:
 add(k,n,l,"1/min" if k.endswith("RATE") else "cyl" if k.endswith("CYL") else "count","NONE",1 if k.endswith("RATE") else 0,op,["MISFIRE_1","MISFIRE_2","MISFIRE_3","MISFIRE_4"])
add("MISFIRE_IDLE","trip.misfires_idle_observed","IDLE MISFIRE","count","NONE",0,"MISFIRE_IDLE",["MISFIRE_TOTAL","ENGINE_RPM","VEHICLE_SPEED"])
add("MISFIRE_LOAD","trip.misfires_load_observed","LOAD MISFIRE","count","NONE",0,"MISFIRE_LOAD",["MISFIRE_TOTAL","ENGINE_LOAD"])
add("TRIM_SUM","engine.fuel_trim_sum_b1","TRIM SUM B1","%","PERCENT",1,"ADD",["STFT","LTFT"])
for k,s,l,op in [("TRIM_AVG","average_10s","TRIM AVG 10S","ROLL_AVG"),("TRIM_MIN","min_10s","TRIM MIN 10S","ROLL_MIN"),("TRIM_MAX","max_10s","TRIM MAX 10S","ROLL_MAX")]:
 add(k,"engine.fuel_trim_"+s,l,"%","PERCENT",1,op,["TRIM_SUM"],10)
add("TRIM_IDLE","engine.fuel_trim_idle","IDLE TRIM","%","PERCENT",1,"IDLE_VALUE",["TRIM_SUM","ENGINE_RPM","VEHICLE_SPEED"])
add("TRIM_LOAD","engine.fuel_trim_load","LOAD TRIM","%","PERCENT",1,"LOAD_VALUE",["TRIM_SUM","ENGINE_LOAD"])
add("BOOST_ERROR","engine.charge_pressure_error","CHARGE ERROR","hPa","PRESSURE",0,"SUB",["ENGINE_CHARGE_PRESSURE","CHARGE_REQUESTED"])
add("BOOST_ABS_ERROR","engine.charge_pressure_abs_error","ABS CHG ERR","hPa","PRESSURE",0,"ABS",["BOOST_ERROR"])
add("BOOST_PEAK","trip.boost_peak","BOOST PEAK","hPa","BOOST",2,"TRIP_MAX",["ENGINE_BOOST"])
add("BOOST_AVG","engine.boost_average_5s","BOOST AVG 5S","hPa","BOOST",2,"ROLL_AVG",["ENGINE_BOOST"],5)
add("OVERBOOST_COUNT","session.overboost_events","OVERBOOST","count","NONE",0,"RULE_COUNT",["BOOST_ERROR"],0)
add("UNDERBOOST_COUNT","session.underboost_events","UNDERBOOST","count","NONE",0,"RULE_COUNT",["BOOST_ERROR"],1)
for key,dep,prefix,label in [("COOLANT","ENGINE_COOLANT_TEMPERATURE","coolant","COOLANT"),("OIL","ENGINE_OIL_TEMPERATURE","oil","OIL"),("IAT","IAT","iat","IAT"),("DSG","TRANSMISSION_OIL_TEMPERATURE","dsg_oil","DSG OIL"),("HALDEX","AWD_CLUTCH_TEMPERATURE","haldex_clutch","HALDEX")]:
 add(key+"_PEAK","trip."+prefix+"_peak",label+" PEAK","degC","TEMPERATURE",0,"TRIP_MAX",[dep])
add("IAT_DELTA","engine.iat_ambient_delta","IAT DELTA","degC","TEMP_DELTA",1,"SUB",["IAT","AIR_TEMP"])
for key,dep,prefix in [("COOLANT","ENGINE_COOLANT_TEMPERATURE","coolant"),("OIL","ENGINE_OIL_TEMPERATURE","oil"),("IAT","IAT","iat")]:
 add(key+"_RATE","engine."+prefix+"_rise_rate",key+" RATE","degC/s","NONE",2,"RATE",[dep])
add("WARMUP","trip.coolant_warmup_time","WARMUP TIME","s","NONE",1,"WARMUP",["ENGINE_COOLANT_TEMPERATURE","ENGINE_RPM"])
add("HEAT_SOAK","engine.heat_soak_proxy","HEAT SOAK ?","","NONE",0,"HEAT_SOAK",["IAT_DELTA","VEHICLE_SPEED","ENGINE_RPM"])
for key,dep,n,param in [("COOLANT","ENGINE_COOLANT_TEMPERATURE","coolant",2),("OIL","ENGINE_OIL_TEMPERATURE","oil",3),("DSG","TRANSMISSION_OIL_TEMPERATURE","dsg_oil",4)]:
 add(key+"_HIGH_TIME","trip."+n+"_high_seconds",key+" HIGH S","s","NONE",1,"RULE_TIME",[dep],param)
add("VOLTAGE_MIN","trip.ecu_voltage_min","ECU V MIN","V","VOLTAGE",2,"TRIP_MIN",["ECU_SUPPLY_VOLTAGE"])
add("VOLTAGE_MAX","trip.ecu_voltage_max","ECU V MAX","V","VOLTAGE",2,"TRIP_MAX",["ECU_SUPPLY_VOLTAGE"])
add("VOLTAGE_AVG","engine.ecu_voltage_average_5s","ECU V AVG","V","VOLTAGE",2,"ROLL_AVG",["ECU_SUPPLY_VOLTAGE"],5)
add("VOLTAGE_DROPS","session.ecu_voltage_drop_events","ECU V DROPS","count","NONE",0,"RULE_COUNT",["ECU_SUPPLY_VOLTAGE"],5)
add("VOLTAGE_LOW_TIME","trip.ecu_voltage_low_seconds","ECU V LOW S","s","NONE",1,"RULE_TIME",["ECU_SUPPLY_VOLTAGE"],5)
for scope in ["SESSION","TRIP"]:
 for key,dep,name,unit,family,dec in [("RPM","ENGINE_RPM","rpm","rpm","NONE",0),("SPEED","VEHICLE_SPEED","speed","km/h","SPEED",0),("BOOST","ENGINE_BOOST","boost","hPa","BOOST",2)]:
  add(scope+"_"+key+"_MAX",scope.lower()+"."+name+"_max",scope+" "+key,unit,family,dec,scope+"_MAX",[dep])
for index,(key,name,label) in enumerate([("050","0_50","0-50"),("0100","0_100","0-100"),("60100","60_100","60-100"),("80120","80_120","80-120"),("100200","100_200","100-200"),("XY","x_y","X-Y")]):
 add("PERF_"+key,"performance.accel_"+name,label+" TIME","s","NONE",2,"PERFORMANCE",["VEHICLE_SPEED"],index)
add("ACCEL_DURATION","performance.acceleration_duration","RUN TIME","s","NONE",2,"RUN_DURATION",["VEHICLE_SPEED"])
add("PERF_COUNT","trip.performance_runs","PERF RUNS","count","NONE",0,"PERF_COUNT",["VEHICLE_SPEED"])
add("TRIP_KNOCK_COUNT","trip.retard_events","TRIP RET EV","count","NONE",0,"TRIP_KNOCK_COUNT",["RETARD_WORST"])
add("EVENT_COUNT","session.analytics_events","EVENTS","count","NONE",0,"EVENT_COUNT",[])
add("LOST_COUNT","session.signal_loss_events","SIGNAL LOST","count","NONE",0,"LOST_COUNT",[])
add("CAN_ERROR_COUNT","session.can_error_events","CAN ERRORS","count","NONE",0,"CAN_ERROR_COUNT",[])
def symbol(k): return "MD_SIGNAL_"+k
enum="/* Generated by scripts/generate_analytics_catalog.py; append-only IDs. */\n"
enum+="".join("    "+symbol(row[0])+",\n" for row in raw)
enum+="    MD_SIGNAL_RAW_COUNT,\n"
# RAW_COUNT is a marker, so first derived must reuse it.
enum+="".join("    "+symbol(row[0])+(" = MD_SIGNAL_RAW_COUNT" if i==0 else "")+",\n" for i,row in enumerate(derived))
defs="/* Generated canonical definitions. */\n"
for row in raw+derived:
 key,name,label,unit,family,dec=row[:6]
 assert len(name)<48
 defs+=f'    {{{symbol(key)}, "{name}", "{label}", "{unit}", MD_UI_UNIT_{family}, {dec}}},\n'
ops=sorted({row[6] for row in derived}|{"SUB"})
registry="/* Generated graph nodes; no CAN/PID/DID bindings. */\n"
registry+='    {MD_SIGNAL_ENGINE_BOOST, MD_DERIVED_SUB, 2, {MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE, MD_SIGNAL_ENGINE_AMBIENT_PRESSURE}, 0},\n'
for row in derived:
 key,*_=row;op,deps,param=row[6:]
 registry+=f'    {{{symbol(key)}, MD_DERIVED_{op}, {len(deps)}, {{'+(", ".join(map(symbol,deps)) or "0")+f'}}, {param}}},\n'
for path,content in [("analytics_signal_ids.inc",enum),("analytics_signal_definitions.inc",defs),("analytics_nodes.inc",registry),("analytics_operations.inc","".join("    MD_DERIVED_"+op+",\n" for op in ops))]:
 (ROOT/"firmware/main"/path).write_text(content)
base_text=(ROOT/"firmware/main/ui_core.c").read_text().split('#include "analytics_signal_definitions.inc"')[0]
base=re.findall(r'\{(MD_SIGNAL_\w+), "([^"]+)", "([^"]+)", "([^"]*)", MD_UI_UNIT_\w+, (\d+)\}',base_text)
assert len(base)==14
names={k:n for k,n,*_ in base};names.update({symbol(r[0]):r[1] for r in raw+derived})
obd={k:(int(pid,16),int(ttl)) for pid,k,ttl in re.findall(r'\{(0x[0-9a-f]+), \d+, (MD_SIGNAL_\w+), \d+, (\d+)\}',(ROOT/"firmware/main/acquisition.c").read_text())}
rows=[]
for index,row in enumerate(base+[(symbol(r[0]),*r[1:4],str(r[5])) for r in raw+derived]):
 key,name,label,unit,dec=row;node=next((r for r in derived if symbol(r[0])==key),None)
 is_derived=node is not None or key=="MD_SIGNAL_ENGINE_BOOST"
 deps=node[7] if node else ["ENGINE_MANIFOLD_PRESSURE","ENGINE_AMBIENT_PRESSURE"] if is_derived else []
 entry=dict(id=index,canonical_name=name,label=label,unit=unit,precision=int(dec),derived=is_derived,dependencies=[names[symbol(d)] for d in deps],operation=node[6] if node else "SUB" if is_derived else None)
 entry["provenance"]="Original MiniDash arithmetic/observed statistics; derived-engine design report" if is_derived else "Standard OBD supported-PID gate" if key in obd else "Candidate/unverified native binding; existing ECU evidence requires codec/address/identity review"
 if key in obd: entry.update(obd_pid=f"01/{obd[key][0]:02X}",ttl_ms=obd[key][1],acquisition_policy="supported_pid_bitmap")
 elif not is_derived: entry.update(acquisition_enabled=False,evidence_required=True)
 rows.append(entry)
out=ROOT/"data/analytics/signals.json";out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(dict(schema=1,source="scripts/generate_analytics_catalog.py",signals=rows),indent=2)+"\n")
if __name__=="__main__": print(f"raw={14+len(raw)} derived={1+len(derived)} total={14+len(raw)+len(derived)}")
