# Wi-Fi coexistence and instrument themes — 2026-10-01

## Delivered behavior

Wi-Fi is an explicit temporary AP, off at startup. Settings row 23 shows ON/OFF
(ВКЛ/ВЫКЛ) and a noninteractive switch indicator; CHANGE toggles the AP.
BACK leaves the credentials screen without stopping Wi-Fi. Settings editing,
brightness, signal rebinding and page navigation remain available with AP on.
Driving lockout remains in place.

There is one configuration revision shared by local UI and HTTP. A local edit
ends an active temporary Web preview, restores its saved baseline, then applies
the local change. It invalidates stale Web drafts: HTTP 409 requires Reload.
Cancel or AP shutdown without an active preview preserves pending local changes.
Before the first preview, pending local changes are checkpointed to NVS; a failed
checkpoint rejects the preview. The asynchronous writer never persists preview
state. Save still validates and commits before publishing.

Themes 0–8 retain their IDs; Volkswagen=9 and Porsche=10 are appended. Existing
configuration schema 2 and legacy v4 introduced by rotation are unchanged.
Terminal/Matrix/Fallout/Amber use JetBrains Mono with Latin/Cyrillic body text;
Cyberpunk also uses Mono. Volkswagen/Porsche use Roboto Condensed for numeric
values and the existing localized body font. Arctic retains sans-serif.

Terminal has square rail frames; Matrix has a thicker phosphor rail; Fallout
and Amber use rounded inset frames; Cyberpunk adds a diagonal corner accent;
Arctic has softer rounded cards. Volkswagen uses black, white, blue and red
accents with restrained bezels; Porsche uses black, warm ivory/gold and red with
larger rounded bezels. Numeric cards with sufficient room draw instrument arcs
and ticks underneath labels. These are decorative frames, not a new gauge widget:
no invented pointer, live data or vehicle-specific scale is presented. Small
cards retain readable numeric layouts. Existing bar/status/graph widgets remain.

`GET /api/themes` now returns compact palette/style resources, retaining the
frontend's `common.palette` path, rather than eleven complete copies of user
pages/settings. This avoids response growth beyond the on-device test capture
limit and reduces allocations. Style metadata includes font family, frame kind
and instrument artwork. Browser preview is schematic with CSS font fallbacks;
the embedded font faces are on the device, not downloaded to the browser.

## Implementation

- `display_app.c`: local/Web takeover and revision policy, AP-independent local
  save queue and rebinding, runtime Wi-Fi diagnostics.
- `ui_screens.c`, `ui_model.h`, `ui_i18n.c`: status switch, localized ON/OFF,
  theme-aware font application/reapplication and card draw callbacks. Artwork
  runs before child labels; fonts are immutable LVGL assets.
- `ui_theme.{h,c}`, `ui_core.h`: appended IDs, palettes and style traits.
- `ui_configuration_json.c`, `webui.html`: compact preset resources and preview.
- `CMakeLists.txt`, `ui_font_mono_*.c`, `ui_font_cluster_*.c`: compiled font assets.
- `ui_web.c`: real MCU HTTP tests for local brightness, revision conflicts,
  cancel preservation, preview takeover and menu theme changes.
- `ui_platform_selftest.c`, `ui_screens.c`, `core/test_webui.cjs`: theme IDs/style
  traits, actual glyph availability and frontend resources.

## Font provenance and regeneration

Sources are OFL-1.1, with license texts included next to generated assets:
[JetBrains Mono](https://github.com/JetBrains/JetBrainsMono),
[Roboto Condensed](https://github.com/google/fonts/tree/main/ofl/robotocondensed).
No trademark logo, proprietary brand font or dashboard artwork was copied.
Volkswagen/Porsche are original interpretations of instrument styling.

Downloaded JetBrains `fonts/ttf/JetBrainsMono-Regular.ttf` SHA256:
`e6fd0d7e91550b3ed2b735d4312474362c4716edc4fc0577a0f61ed782d5aed1`.
Roboto `RobotoCondensed[wght].ttf` SHA256:
`dace262afcee68a5276f200d8026c57221735c0118ab5fda8c2c0d3dc409a8d0`.
License SHA256: Mono `a76abf002c49097d146e86740a3105a5d00450b1592e820a1109a8c5680cd697`;
Condensed `0e4cc6ece88573545be2ed25835363662a6182ba4a4c1b5c8feda52add30e8a6`.

Generate using official [lv_font_conv](https://github.com/lvgl/lv_font_conv) 1.5.3:
`--font SOURCE --size N --bpp 4 --no-compress --format lvgl --lv-include lvgl.h
--lv-font-name md_ui_font_FAMILY_N --force-fast-kern-format -r RANGE -o OUTPUT`.
Mono sizes 10/12/14/16 include `0x20-0x7F,0xB0,0x2022,0x400-0x45F`;
Mono 20/28/48 and Condensed 16/20/28/48 use `0x20-0x7F,0xB0`.
Headers replace the host-specific generated command with a neutral provenance
comment. Runtime needs neither font converter nor source TTF files.

## Verification

Native CMake/CTest: 3/3 pass, including portable UI/configuration contracts and
JSON. Node frontend logic test passes with all eleven resources and portrait
geometry. ESP-IDF 5.5.5 build uses LVGL 9.5.0 and esp_lvgl_port 2.9.0.

Final app-only COM5 flash at offset `0x10000` passed esptool written-hash
verification. Binary size **1,884,176 bytes**, SHA256
`a67ee9959841c7da2c58335dced2128ca09972a3b01d17b7ff1d834baac3308e`;
40% of the 3 MiB app partition remains free. No bootloader, partition table,
NVS or calibration erase was performed. Build log has no compiler warnings/errors.
The tested image has the pre-commit version string; source/docs commit does not
change the tested image.

Device tests: UI **27/27 PASS**, actual LVGL text-cache/single-selection/font-glyph
regression **PASS**, real AP/HTTP **33/33 PASS**. Local brightness, normal Settings
theme actions, stale revision rejection, preview takeover and cancel preservation
all pass. Captured common/platform configuration was restored exactly. Calibration
NVS record comparison passes. One AP cycle logged internal free heap 120,475
before / 111,203 after, minimum 27,536 bytes; retained rendering/font allocations
can differ after visiting a new theme, so this is not a long-run leak assertion.
Evidence is in this chat's `work/themes-wifi-*`.
An earlier test correctly detected response capture overflow in the full-copy
theme endpoint; compact resources fixed its cause. Font checks use actual LVGL
glyph descriptors for Cyrillic body letters, digits and degree signs. This is
not a physical typography/readability review.

Final semantic-action bench sweep exercised all **44 combinations** (11 themes
at 0/90/180/270), followed by actual LVGL regression and reboot persistence.
Common/platform configuration was preserved exactly; restored baseline: OEM,
relative 0°, brightness 80%, Russian language, Minimal dashboard and the original
six-widget Custom page. No panic, stack-canary, assertion or failed scene rebuild
appeared in captured logs. Evidence: `work/themes-angles-result.json` and
`work/themes-angles-device.log`. Physical touch/appearance acceptance is pending.

## Remaining checks / next steps

P0 — Owner panel acceptance: with AP on, change theme/brightness/units using
physical touch; verify Wi-Fi switch matches AP state and all four rotations hit
the intended rows. Check no Text placeholders or multiple selected rows return.
Also inspect all themes at 10% and normal brightness in both orientations.
Components: XPT2046 mapping, Settings renderer, font metrics and artwork.
Acceptance: readable values without overlap, correct touch targets, local
changes immediately visible and retained after reboot. Needs physical observation;
MCU action/HTTP tests cannot substitute for it.

P1 — Phone/browser visual acceptance and sustained AP/rotation stress: verify
Preview/Save/Cancel and Reload after local takeover, inspect CSS preview and
monitor memory/render timing over a longer run. Existing buffers and revision
gates limit transactions; long-run performance has not been measured.

P2 — A true configurable gauge widget can be designed separately with meaningful
canonical ranges, fresh/stale behavior and numerical labels. Current decorative
arcs do not claim a live instrument scale. Vehicle acquisition and safety-limit
validation remain outside this UI work; no CAN configuration or vehicle write
path was changed.

Recovery: use `config-export` for current JSON. Normal app-only flashing keeps
NVS and touch calibration. `config-legacy` is an explicit recovery action with
AP off, replacing newer page configuration; it is never invoked by normal
migration or tests. Unknown/invalid records are retained, not erased.
