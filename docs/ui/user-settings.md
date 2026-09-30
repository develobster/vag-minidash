# MiniDash embedded UI settings

Wi-Fi/theme update (2026-10-01): local settings remain editable with AP enabled;
Wi-Fi shows an ON/OFF indicator. Eleven themes now include Volkswagen/Porsche,
monospaced retro fonts and distinct frames/instrument artwork. See
[implementation, tests and remaining checks](wifi-and-instrument-themes-2026-10-01.md).

Rotation update (2026-09-30): Settings supports 0/90/180/270 degrees, with
portrait card/grid reflow and transformed calibrated touch. Configuration schema
2 migrates old build-only orientation to relative 0 without changing common
settings. See [implementation, migration and verification](rotation-and-portrait-2026-09-30.md).

Theme/startup update (2026-09-30): nine presets including Terminal, Matrix,
Fallout, Amber CRT, Cyberpunk and Arctic; boot-only CRT reveal with autonomous
LEDC fade (~2 s) to saved brightness; new-settings default is now 70%.
See [implementation and acceptance](themes-and-crt-startup-2026-09-30.md).

Current backlight update (2026-09-30): GPIO4 is owner-confirmed and enabled for
LEDC PWM (5 kHz, 12 bit, gamma 2.0), using the existing Brightness setting and
NVS. Manual Settings control, 10% readable minimum, reboot persistence and cold
USB boot have passed physical bench acceptance. Prior FIXED/3V3 descriptions
below record the preceding wiring. See [backlight implementation and acceptance](../hardware/backlight-pwm-2026-09-30.md).


## Navigation

The XPT2046 touch reader feeds the same semantic UI actions as the console and
future button adapters. After calibration, normal screens use broad left/right
edge zones for previous/next, a tap on a telemetry card for Focus View, and a
swipe as an additional navigation path. Touch coordinates are calibrated into
the current 320x240 landscape orientation before hit testing. Without a valid
calibration, full-screen tap/double-tap/hold navigation remains available.

On calibrated Settings pages, tapping a row only selects it. `CHANGE` applies
the selected setting; `START` launches touch calibration. `MORE` and `PREV`
move between pages. A page indicator shows the current page. The control must
be pressed and released inside the same hit target.

The USB serial console accepts `next`, `prev`, `settings`, `up`, `down`,
`plus`, `minus`, `select`, `back`, `home`, `sleep` and `wake`. These map to
semantic actions and do not contain GPIO or vehicle bus identifiers.

## On-screen options

Settings has 24 rows across six pages (four rows per page). The Language row switches between English and Russian. It sits before
the Custom widget selectors, which now occupy rows 15–22. The page control
continues through each page with one MORE tap.

The other rows include Dashboard, Theme, Day/night, speed units, Temperature,
Brightness, global and per-signal warning switches, touch calibration and a
dashboard layout selector.

Dashboard choices are OEM, Sport, Minimal and Custom. Themes are OEM, Sport, Minimal, Terminal, Matrix, Fallout, Amber CRT,
Cyberpunk, Arctic, Volkswagen and Porsche. Day/night choices are Auto, Day and Night; Auto currently selects the
night-safe palette because no ambient-light or vehicle illumination source is
connected. Brightness changes GPIO4 PWM immediately over 10–100%; sleep uses zero.
Boot restores the saved value with the CRT reveal. Rotation is row 24;
Wi-Fi is row 23 and shows ON/OFF plus a switch indicator.

Warnings are off by default. The four per-signal rows only enable existing
firmware threshold presets. Thresholds are not editable on screen and have not
been validated on a vehicle. They apply only to fresh valid samples.

## Custom dashboard layouts

The Custom dashboard supports 1, 2, 4, 6 or 8 widgets, plus mixed layouts with
1 large and 2 small, 1 large and 4 small, or 2 large and 4 small widgets. The
layout controls geometry; separate slot settings bind each active widget to a
canonical signal. The selector uses only signals in the firmware vocabulary.
Values without a provider remain unavailable; layout selection does not
simulate them.

Press and hold any telemetry card on OEM, Sport, Minimal, Performance,
Temperatures, Drivetrain or Custom to open a signal picker for that card. Tap a
signal to replace its binding; BACK or a long press cancels. Fixed screens keep
separate per-card overrides; Custom keeps its existing widget bindings. The
choice is stored in NVS. A normal card tap still opens Focus View. Rebinding
changes only the UI signal and does not send anything to the vehicle.

Russian localization covers screen headings, dashboard labels, setting names
and values, card statuses, picker options and calibration instructions. The
firmware includes Cyrillic font subsets at four sizes. Their glyph bitmaps are
uncompressed to match this LVGL build, where compressed-font support is off;
the firmware self-test checks glyph coverage and format.

The layouts use a 12-column by 8-row grid. Eight widgets use two columns and
four rows of compact cards; six use a compact 2-by-3 grid; mixed 2-large/4-small
uses 80-pixel-wide lower cards. Narrow cards use short one-line signal aliases
such as `RPM`, `SPD` and `LOAD`; Focus View retains the full signal name. These
layouts pass geometry and NVS checks, but their readability and touch accuracy
on the real panel have not yet been confirmed. Numeric, bar, quality-status and short-history graph renderers are implemented. Up to four Custom pages can be composed with the local Web editor. Dial/gauge widgets remain future work. Wi-Fi configuration is row 23 on page 6; select it then CHANGE. Preview switches to Custom, while Save/Cancel restores the previous view. See [configuration](configuration.md).

## Persistence

Settings are validated and stored in a bounded schema-2 CRC configuration under minidash_ui/configuration. Legacy 393-byte v1, 503-byte v2 and 528-byte v3/v4 records remain readable and retained; touch calibration is independent. Writes are deferred until 1.5 seconds after the
latest edit. Invalid records fall back to defaults without automatic erase.
The NVS partition is declared in `firmware/partitions.csv`.

The Web editor also changes numeric warning thresholds, colors, labels, precision, ranges and widget types. Warnings use fresh samples only and are not vehicle-validated safety limits. Local settings and card rebinding remain available while AP is active. A local edit discards any temporary Web preview; stale Web drafts receive HTTP 409 and must reload.
