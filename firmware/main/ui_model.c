#include "ui_model.h"
#include <math.h>
#include <string.h>

void md_ui_history_observe(md_ui_history_t *h, const md_ui_signal_record_t *r, uint32_t now_ms)
{
    if (!h || !r) return;
    if (!r->received) { h->valid=false; return; }
    const bool valid = r->state == MD_UI_SIGNAL_VALID && isfinite(r->value) &&
        r->ttl_ms != 0 && now_ms - r->updated_ms < r->ttl_ms;
    if (!valid) { h->valid = false; return; }
    if (h->seen && h->last_update_ms == r->updated_ms && h->valid && h->demo == r->demo) return;
    /* A gap or a source/demo transition starts a new statistics window. */
    if (!h->valid || h->demo != r->demo) { h->count = h->head = 0; h->seen = false; }
    if (!h->seen) h->minimum = h->maximum = r->value;
    if (r->value < h->minimum) h->minimum = r->value;
    if (r->value > h->maximum) h->maximum = r->value;
    h->samples[h->head] = r->value;
    h->head = (uint8_t)((h->head + 1U) % MD_UI_HISTORY_SAMPLES);
    if (h->count < MD_UI_HISTORY_SAMPLES) ++h->count;
    h->last_update_ms = r->updated_ms;
    h->seen = h->valid = true;
    h->demo = r->demo;
}
void md_ui_project_widget(md_ui_widget_view_t *v, md_ui_signal_id_t signal,
                         const md_ui_settings_t *settings,
                         const md_ui_signal_store_t *store, uint32_t now_ms,
                         float low, float high, const md_ui_history_t *history)
{
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->signal = md_ui_signal_snapshot(store, signal, now_ms);
    v->severity = v->signal.value_available
        ? md_ui_signal_severity(settings, signal, v->signal.value, v->signal.state)
        : MD_UI_SEVERITY_NORMAL;
    v->inverse = v->severity >= MD_UI_SEVERITY_WARNING || v->signal.state == MD_UI_SIGNAL_ERROR;
    md_ui_format_signal(md_ui_signal_definition(signal), &v->signal, settings,
                        v->value, sizeof(v->value), v->unit, sizeof(v->unit));
    v->has_range = v->signal.value_available && isfinite(low) && isfinite(high) && high > low;
    if (v->has_range) {
        float ratio = (v->signal.value - low) / (high - low);
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        v->level_per_mille = (uint16_t)(ratio * 1000.0f);
    }
    if (history && history->valid && history->seen && v->signal.value_available) {
        v->has_history = true;
        v->minimum = history->minimum;
        v->maximum = history->maximum;
    }
}
bool md_ui_presentation_slice(uint8_t total, uint8_t capacity, uint8_t page,
                              uint8_t *first, uint8_t *count)
{
    if (!first || !count || !total || total > MD_UI_MAX_WIDGETS || !capacity ||
        (unsigned)page * capacity >= total) return false;
    *first = (uint8_t)(page * capacity);
    *count = total - *first < capacity ? total - *first : capacity;
    return true;
}
void md_ui_button_init(md_ui_button_t *b, md_ui_action_t short_action, md_ui_action_t long_action)
{
    if (!b) return;
    memset(b, 0, sizeof(*b));
    b->short_action = short_action; b->long_action = long_action;
}
bool md_ui_button_sample(md_ui_button_t *b, bool pressed, uint32_t now,
                         bool lockout, md_ui_action_t *action)
{
    if (!b || !action) return false;
    if (pressed != b->raw_pressed) { b->raw_pressed = pressed; b->raw_changed_ms = now; }
    if (lockout) { b->suppressed_until_release = true; b->long_sent = true; return false; }
    if (b->suppressed_until_release) {
        if (!pressed && now - b->raw_changed_ms >= 30U) {
            b->stable_pressed = false; b->suppressed_until_release = false;
        }
        return false;
    }
    if (now - b->raw_changed_ms < 30U) return false;
    if (b->stable_pressed != pressed) {
        b->stable_pressed = pressed;
        if (pressed) { b->pressed_ms = b->repeat_ms = now; b->long_sent = false; }
        else if (!b->long_sent) { *action = b->short_action; return true; }
    }
    if (!b->stable_pressed) return false;
    if (!b->long_sent && now - b->pressed_ms >= 800U) {
        b->long_sent = true; b->repeat_ms = now; *action = b->long_action; return true;
    }
    if (b->long_sent && md_ui_action_repeatable(b->long_action) && now - b->repeat_ms >= 180U) {
        b->repeat_ms = now; *action = b->long_action; return true;
    }
    return false;
}
bool md_ui_vehicle_button_action(bool verified, md_ui_action_t mapped, md_ui_action_t *action)
{
    if (!verified || !action || !md_ui_input_valid(MD_UI_INPUT_STEERING_WHEEL, mapped)) return false;
    *action = mapped; return true;
}
void md_brightness_init(md_brightness_t *b, const md_target_profile_t *target, uint8_t level)
{
    if (!b) return;
    *b = (md_brightness_t){.minimum = 10, .maximum = 100, .target = level > 100 ? 100 : level,
        .current = level > 100 ? 100 : level, .dimming = md_target_has(target, MD_CAP_DIMMING),
        .ambient = md_target_has(target, MD_CAP_AMBIENT_SOURCE), .screen_on = true};
    b->mode = b->dimming ? MD_BRIGHTNESS_MANUAL : MD_BRIGHTNESS_FIXED;
}
bool md_brightness_set(md_brightness_t *b, uint8_t level)
{
    if (!b || !b->dimming || level > b->maximum || level < b->minimum) return false;
    b->target = level; return true;
}
bool md_brightness_mode(md_brightness_t *b, md_brightness_mode_t mode)
{
    if (!b || mode < MD_BRIGHTNESS_FIXED || mode > MD_BRIGHTNESS_AUTO ||
        (mode != MD_BRIGHTNESS_FIXED && !b->dimming) ||
        (mode == MD_BRIGHTNESS_AUTO && !b->ambient)) return false;
    b->mode = mode; return true;
}
bool md_brightness_tick(md_brightness_t *b, uint32_t now, uint8_t *output)
{
    if (!b || !output || !b->dimming || now - b->last_step_ms < 20U) return false;
    const uint8_t goal = b->screen_on ? b->target : 0;
    if (goal == b->current) return false;
    b->last_step_ms = now;
    b->current += goal > b->current ? 1 : -1;
    *output = b->current; return true;
}
