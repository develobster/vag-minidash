#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Names and freshness semantics mirror minidash_live.runtime.SignalStore. */
typedef enum {
    MD_RPM, MD_SPEED, MD_OIL_TEMP, MD_COOLANT_TEMP,
    MD_CHARGE_PRESSURE, MD_AMBIENT_PRESSURE, MD_BATTERY_VOLTAGE,
    MD_SIGNAL_COUNT
} md_signal_id_t;

typedef struct {
    const char *canonical_name;
    const char *unit;
    float value;
    uint32_t timestamp_ms;
    uint32_t ttl_ms;
    bool valid;
} md_signal_t;

typedef struct { md_signal_t values[MD_SIGNAL_COUNT]; } md_signal_store_t;

typedef enum { MD_PAGE_DRIVE, MD_PAGE_TEMPERATURES, MD_PAGE_COUNT } md_page_t;
typedef enum { MD_INPUT_TOUCH, MD_INPUT_BUTTON, MD_INPUT_CAN, MD_INPUT_CONSOLE } md_input_source_t;
typedef enum { MD_NAV_NEXT, MD_NAV_PREVIOUS, MD_NAV_SLEEP, MD_NAV_WAKE } md_navigation_t;

typedef struct { md_page_t page; bool sleeping; } md_input_manager_t;

typedef struct {
    md_page_t page;
    bool sleeping;
    bool available[MD_SIGNAL_COUNT];
    float value[MD_SIGNAL_COUNT];
    bool boost_available;
    float boost_bar;
} md_ui_model_t;

void md_store_init(md_signal_store_t *store);
void md_store_publish(md_signal_store_t *store, md_signal_id_t id, float value,
                      uint32_t now_ms, uint32_t ttl_ms);
void md_demo_publish(md_signal_store_t *store, uint32_t now_ms);
void md_input_dispatch(md_input_manager_t *input, md_input_source_t source, md_navigation_t action);
void md_ui_project(const md_signal_store_t *store, const md_input_manager_t *input,
                   uint32_t now_ms, md_ui_model_t *model);
