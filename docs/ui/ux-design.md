# MiniDash 320×240 interaction decision

Status: chosen for the `feature/ui-ux-touch-zones` implementation. The verified panel mode is 320×240 landscape, rotation 2; no portrait layout is assumed. Vehicle acquisition and CAN identifiers remain outside the UI.

## Focused research

* [Android for Cars visual principles](https://developers.google.com/cars/design/design-foundations/visual-principles) emphasize glanceable information, a clear visual hierarchy, large nonoverlapping targets and restrained night brightness. Its dp sizes cannot be copied as pixel numbers onto this 2.4-inch display.
* [CarPlay HIG](https://developer.apple.com/design/human-interface-guidelines/carplay) favors high-value information, few interactions and consistent controls. MiniDash is an instrument, so its dashboard is Home rather than an app launcher.
* [CANShift firmware](https://github.com/CANShift/canshift-firmware) is a close ESP32/LVGL/320×240/XPT2046/CAN reference. It keeps acquisition separate from the UI and persists four-point touch calibration in NVS. We adopt those boundary and calibration lessons, not its configurable dashboard editor or vehicle actions.
* [ESP32 C3 OBD Gauge](https://github.com/antoxa2584x/esp32-c3-obd-gauge) uses swipe-only page navigation. This preserves space but requires a learned gesture; MiniDash needs a discoverable one-tap route as well.
* [EV Info Display](https://github.com/danjulio/ev_info_display) and [OBD BRZ Gauge](https://github.com/steveEcode/obd_brz_gauge) show the value of small instrument-specific screen sets and a UI task fed by a separate vehicle data source.

These sources informed interaction priorities. They do not validate MiniDash touch calibration, vehicle signals or thresholds.

## Three 320×240 mock layouts

| Variant | Mock | Glanceability | Density | Touch and discovery | Space used by navigation |
| --- | --- | --- | --- | --- | --- |
| A — persistent bottom bar | [SVG](ux-variant-a-bottom-bar.svg) | Good | Reduced | Obvious controls, large targets | 44 px high, about 18% of screen |
| B — swipe only | [SVG](ux-variant-b-swipe.svg) | Excellent | Highest | Gesture must be learned; less reliable under vibration | Minimal |
| C — hybrid edge zones | [SVG](ux-variant-c-edge-zones.svg) | Excellent | High | One-tap left/right zones, small visible chevrons, optional swipe | No permanent bar |

**Decision: C.** Use 56-pixel-wide left and right hit zones across normal telemetry screens, with small visible chevrons and immediate pressed feedback. A release in the same zone navigates once; a large movement may navigate as a swipe. The central area retains data-bound telemetry cards. A card tap opens one generic Focus View for its canonical signal. In Focus View, a tap or BACK returns to the originating screen. Holding a card on OEM, Sport, Minimal, Performance, Temperatures, Drivetrain or Custom opens a signal picker for that card; tapping a signal replaces only that binding. Long press on non-card background opens Settings. Dashboard is Home. There is no permanent BACK/HOME/NEXT/PREV/SELECT strip.

Card hit testing has lower priority than edge navigation and uses each card's actual layout bounds; labels that are not cards remain informational. The left/right hit zones may cover part of an edge card, so the center of each card must remain large enough to tap. The development overlay shows the exact zones, last mapped touch coordinate and current target. It is disabled in normal builds.

Existing Dashboard, Performance, Temperatures, Drivetrain, Diagnostics, Custom and Settings screens remain. Dashboard/Performance and Dashboard/Custom overlap in content; bidirectional navigation and direct Focus View reduce the number of interactions while future vehicle data clarifies whether to move Diagnostics or Custom into a secondary menu. Do not invent new gauge data to fill space.

## Semantic input and policy

Touch zones, cards, gestures, console, future GPIO buttons and evidenced vehicle-button adapters produce the same semantic actions: NEXT, PREVIOUS, BACK, HOME, SELECT and OPEN_DETAILS(signal_id). Hardware identifiers stay in adapters. A future driving-context policy can restrict calibration, settings and diagnostic writes when a trusted PARKED/DRIVING source exists. Until then the context is UNKNOWN and no vehicle write action is available. Long press fires once; repeat is disabled for selection, settings and future destructive actions. Touch gestures supplement one-tap actions. The signal picker also accepts NEXT_ITEM, PREVIOUS_ITEM, SELECT and BACK so future buttons can use the same flow.

The Focus View shows signal name, dominant current value, unit and unobtrusive quality. It accepts any signal in the firmware's canonical vocabulary and never interprets raw CAN/UDS data. VALID shows the value; STALE, UNAVAILABLE and ERROR show a distinct state with no plausible live value. The existing warning model remains disabled by default; no threshold is invented here. Day/night retains a dark dashboard, and AUTO remains unconnected until an ambient sensor or verified vehicle illumination source exists.

Settings uses a different interaction pattern because changing a value is a separate intent from selecting it. A tap anywhere on a row only highlights that row. Twenty-two settings are shown on six pages of four rows (the last page has two rows). Three explicit 40-pixel-high controls at the bottom provide PREV, MORE and CHANGE; PREV is hidden on the first page, MORE on the last. CHANGE applies to the highlighted row; the calibration row labels it START. A page indicator shows PAGE 1/6, etc. The Language row switches between English and Russian. The layout and widget rows select among 1/2/4/6/8 and mixed presets, then bind active Custom slots to canonical signal names. Per-signal warning toggles use the existing disabled-by-default threshold presets; those values are not vehicle-validated safety limits. The page controls replace the former edge-page shortcut, which could intercept taps on row labels. A press must begin and end inside the same control. The uncalibrated gesture fallback and semantic console SELECT remain available.

## Calibration and physical constraints

The fitted controller is the marked XPT2046. Full-screen gestures work without coordinates, but location-based controls require a raw-X/Y to logical-screen affine calibration. The on-device wizard uses multiple targets, rejects degenerate/noisy fits, stores a versioned record with panel geometry and CRC in NVS, and can be reopened from Settings. Uncalibrated units retain the prior gesture route so the wizard is reachable without a PC. Manual brightness uses owner-confirmed GPIO4 LEDC PWM with saved settings and recorded bench acceptance. AUTO still needs an evidenced ambient source. Electrical load/PWM/temperature acceptance for permanent installation remains separate; see [backlight evidence](../hardware/backlight-pwm-2026-09-30.md).

## Acceptance on the real panel

Check Dashboard readability at a short glance; tap left/right edges without aiming at the chevrons; tap cards from their central area; return from Focus in one action; verify visible pressed feedback, Settings access and calibration restart. Check all four corners and center after calibration, reboot persistence, stale/error rendering and a debug overlay build. Keep CAN disabled for this UI acceptance.
