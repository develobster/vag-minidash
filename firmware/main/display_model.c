#include "display_model.h"
#include <math.h>
#include <string.h>

static const char *const names[MD_SIGNAL_COUNT] = {
    "engine.rpm", "vehicle.speed", "engine.oil_temperature",
    "engine.coolant_temperature", "engine.charge_pressure_actual",
    "engine.ambient_pressure", "vehicle.battery_voltage"
};
static const char *const units[MD_SIGNAL_COUNT] = {
    "rpm", "km/h", "degC", "degC", "hPa", "hPa", "V"
};

void md_store_init(md_signal_store_t *store)
{
    memset(store, 0, sizeof(*store));
    for (int i = 0; i < MD_SIGNAL_COUNT; ++i) {
        store->values[i].canonical_name = names[i];
        store->values[i].unit = units[i];
    }
}

void md_store_publish(md_signal_store_t *store, md_signal_id_t id, float value,
                      uint32_t now_ms, uint32_t ttl_ms)
{
    if (!store || id < 0 || id >= MD_SIGNAL_COUNT || ttl_ms == 0) return;
    md_signal_t *row = &store->values[id];
    row->value = value;
    row->timestamp_ms = now_ms;
    row->ttl_ms = ttl_ms;
    row->valid = isfinite(value);
}

void md_demo_publish(md_signal_store_t *store, uint32_t now_ms)
{
    md_store_publish(store, MD_RPM, 2450, now_ms, 3000);
    md_store_publish(store, MD_SPEED, 73, now_ms, 3000);
    md_store_publish(store, MD_OIL_TEMP, 96, now_ms, 3000);
    md_store_publish(store, MD_COOLANT_TEMP, 91, now_ms, 3000);
    md_store_publish(store, MD_CHARGE_PRESSURE, 1713, now_ms, 3000);
    md_store_publish(store, MD_AMBIENT_PRESSURE, 993, now_ms, 3000);
    md_store_publish(store, MD_BATTERY_VOLTAGE, 14.3f, now_ms, 3000);
}

void md_input_dispatch(md_input_manager_t *input, md_input_source_t source, md_navigation_t action)
{
    if (!input || source < MD_INPUT_TOUCH || source > MD_INPUT_CONSOLE) return;
    switch (action) {
    case MD_NAV_NEXT: if (!input->sleeping) input->page = (input->page + 1) % MD_PAGE_COUNT; break;
    case MD_NAV_PREVIOUS: if (!input->sleeping) input->page = (input->page + MD_PAGE_COUNT - 1) % MD_PAGE_COUNT; break;
    case MD_NAV_SLEEP: input->sleeping = true; break;
    case MD_NAV_WAKE: input->sleeping = false; break;
    }
}

void md_ui_project(const md_signal_store_t *store, const md_input_manager_t *input,
                   uint32_t now_ms, md_ui_model_t *model)
{
    memset(model, 0, sizeof(*model));
    model->page = input->page;
    model->sleeping = input->sleeping;
    for (int i = 0; i < MD_SIGNAL_COUNT; ++i) {
        const md_signal_t *row = &store->values[i];
        model->available[i] = row->valid && row->ttl_ms > 0 &&
            (uint32_t)(now_ms - row->timestamp_ms) < row->ttl_ms;
        if (model->available[i]) model->value[i] = row->value;
    }
    if (model->available[MD_CHARGE_PRESSURE] && model->available[MD_AMBIENT_PRESSURE]) {
        model->boost_available = true;
        model->boost_bar = (model->value[MD_CHARGE_PRESSURE] -
                            model->value[MD_AMBIENT_PRESSURE]) / 1000.0f;
    }
}
