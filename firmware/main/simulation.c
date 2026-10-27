#include "simulation.h"
#include <math.h>
bool md_simulation_value(md_ui_signal_id_t id, uint32_t elapsed_ms, double *value)
{
    if (!value) return false;
    const double wave=(1.0-cos((elapsed_ms%30000U)*6.283185307179586/30000.0))/2.0;
    switch(id) {
    case MD_SIGNAL_ENGINE_RPM: *value=850+wave*4350; break;
    case MD_SIGNAL_VEHICLE_SPEED: *value=wave*130; break;
    case MD_SIGNAL_ENGINE_COOLANT_TEMPERATURE: *value=85+wave*10; break;
    case MD_SIGNAL_ENGINE_OIL_TEMPERATURE: *value=95+wave*15; break;
    case MD_SIGNAL_ENGINE_LOAD: *value=15+wave*80; break;
    case MD_SIGNAL_ENGINE_CHARGE_PRESSURE:
    case MD_SIGNAL_ENGINE_MANIFOLD_PRESSURE: *value=1000+wave*1400; break;
    case MD_SIGNAL_ENGINE_AMBIENT_PRESSURE: *value=1000; break;
    case MD_SIGNAL_ENGINE_BOOST: *value=wave*1400; break;
    case MD_SIGNAL_TRANSMISSION_OIL_TEMPERATURE: *value=70+wave*18; break;
    case MD_SIGNAL_TRANSMISSION_GEAR: *value=1+(unsigned)(wave*5); break;
    case MD_SIGNAL_AWD_CLUTCH_TEMPERATURE: *value=45+wave*18; break;
    case MD_SIGNAL_BATTERY_VOLTAGE:
    case MD_SIGNAL_ECU_SUPPLY_VOLTAGE: *value=13.8+wave*0.6; break;
    default: return false;
    }
    return true;
}
