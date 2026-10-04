# Screen rotation and portrait layouts — 2026-09-30

## Delivered behavior and usage

Settings now contains **Screen rotation / Поворот экрана**, appended as row 23
(zero-based) on page 6, beside the existing Wi-Fi setting. CHANGE cycles
0° → 90° → 180° → 270° → 0°. Console plus/minus supports both directions.
The Web editor General section exposes the same four angles, including temporary
Preview, persistent Save and restoration through Cancel.

0° is the verified, previously working board orientation. Angles are clockwise
relative to it, not the raw panel MADCTL/build profile. 0° and 180° use 320×240;
90° and 270° use 240×320 on the connected ST7789. Half-turn changes leave the
existing LVGL objects, layouts and widget assignments in place. A change between
landscape and portrait rebuilds the scene for its new dimensions and can take
about 1.5 seconds on this SPI prototype. No restart is required.

Portrait layouts reflow rather than squeeze the original landscape scene:

- OEM, Sport and Minimal: full-width main gauge above two secondary cards and
  a full-width bottom card; numbers remain upright and preserve signal bindings.
- Performance, Temperatures and Drivetrain: two columns with taller cards.
- Custom layouts: transpose grid presentation from 12×8 to 8×12, including
  preset and manually arranged widgets. Stored columns/rows are unchanged, so
  returning to landscape recovers the original geometry and ordering.
- Settings: stack the label and value within each row to fit Russian text;
  navigation, CHANGE and touch hit regions follow actual LVGL geometry.
- Focus, calibration, service diagnostics, Wi-Fi and widget picker resize to the
  logical screen dimensions. No telemetry or sensor readings are invented.

Touch calibration is retained in the original, verified board coordinate system.
Each press/release is transformed into the current logical orientation. Swipes,
edge navigation, card details, Settings and picker selections use logical sizes.
If calibration is run in portrait or upside-down mode, displayed target points
are transformed back before fitting/saving the original affine calibration.
Changing angle clears the previous press, avoiding a release being interpreted
against the previous orientation. Touch pins and GPIO4 backlight wiring stay as
verified. CRT startup uses the already rotated scene and saved brightness.

## Implementation and contracts

- `ui_core.{h,c}`: quarter-turn `rotation` in runtime settings; row appended to
  preserve previous indices; default 0; bounds 0–3; dispatch and changed detection.
- `ui_configuration.{h,c}`: propagate rotation in both directions and save it in
  the existing platform field. The old unconditional build-time overwrite is
  removed from `display_app.c`.
- `display_app.c`: use LVGL display rotation and esp_lvgl_port's hardware panel
  update callback; apply stored orientation before screen creation/CRT startup.
  Update on the UI controller under the LVGL lock. Complete pending refresh
  before panel rotation; rebuild only when width/height actually change. Panel
  baseline, gaps and driver-specific mapping remain the verified build profile.
- `ui_screens.{h,c}`: release old scene roots through a temporary active screen,
  recreate permanent static views, reapply palette/configuration, then show the
  previous logical screen. Theme, signal history and calibration are retained.
- `ui_model.{h,c}`: shared portable rectangle calculations for portrait fixed
  cards and transposed widgets; actual screen scaling and font choice happen
  afterwards. Metric labels use compact fonts on physically narrow cards.
- `ui_touch_map.{h,c}`: invertible quarter-turn point conversion; no calibration
  coefficient rewrites when merely switching orientation.
- `ui_configuration_json.c`, `webui.html`: JSON migration, editable platform angle
  and corresponding preview grid; device validation continues to reject invalid
  values. Common-only import still preserves local platform settings.
- `ui_i18n.c`: Russian rotation label. Existing fonts already contain degree signs.
- `ui_platform_selftest.{h,c}`, `ui_core_selftest.c`, `core/tests.c`,
  `core/test_webui.cjs`: portable and on-device tests described below.

### Migration and compatibility

Unified configuration schema **2** gives platform.rotation its new meaning:
0–3 quarter turns relative to the board default. CRC-protected schema-1 records
are accepted, preserve all common data and brightness, and map the previous
build-only rotation field to 0. Old firmware always replaced that field with the
build setting, so using its old stored 2 as a new relative 180° would incorrectly
flip the screen on upgrade. The migration deliberately avoids that.

Legacy settings wire version **4** retains the 528-byte size and widget/warning
positions. Header byte 15 stores language in bits 0–1 and rotation in bits 2–3.
Versions 1, 2 and 3 remain readable and default to 0°; version 3 keeps language
and fixed-card overrides. Unsupported versions, angles or reserved bits fail
validation. Configuration schema 1 is also accepted by JSON parsing/import;
new exports use schema 2. Neither upgrade erases NVS or touch records. The normal
1500 ms settings debounce persists changes after interaction; wait before power
removal. No NVS write occurs on every render or every touch sample.

Downgrading to pre-schema-2 firmware requires care: older firmware does not decode
the new configuration record. Preserve/export the configuration before rollback
and use a converted schema-1 record with the old build rotation or restore the
previous NVS backup. Otherwise the old program can fall back to its separate old
legacy settings record. Do not erase NVS as a routine recovery step. Previous
source baseline is `34ed674` on `feature/real-car-validation`.

## Verification

Environment: Windows PowerShell, ESP-IDF 5.5.5, LVGL 9.5, existing ESP32-S3,
ST7789 and calibrated XPT2046, COM5. No dependencies or new repository were added.

- Native: `cmake --build core/build-consolidation`, then
  `ctest --test-dir core/build-consolidation --output-on-failure`: **3/3 PASS**.
  Portable suite contains 10 tests including rotation. All four angles exercise
  settings/config CRC round trips, next/previous wrap, original corner mappings,
  point inverse mappings, and preservation of language. Schema-1 migration is
  checked against canonical re-encoded wire, not C padding/unused widget slots.
  Legacy version-3 migration is tested separately.
- All fixed portrait card groups and all eight custom layout presets are tested
  for scene bounds and pairwise non-overlap, in both aspect ratios. Existing
  device legacy v1/v2 migration tests continue to run.
- Web: `minidash-json-tests.exe fixture` and
  `node core/test_webui.cjs <fixture>`: PASS. Checks four options, portrait and
  landscape preview grids, rotation Preview/Cancel/Save, and previous editor
  contracts. This is a frontend logic test, not a visual browser screenshot.
- Firmware: `idf.py -B build-backlight build` from `firmware`; final build passed
  without captured compiler warnings/errors. App-only esptool COM5 flash at
  offset `0x10000` passed written-hash verification; bootloader/partitions/NVS
  were not flashed. Binary size **1741472 bytes**, SHA-256
  `5fcf03f3ed3c08739ba46de6c69f3a4e950211a88ed60cc162a624d35a53ea91`.
- Corrected rotation firmware on COM5: device UI 27/27 and actual LVGL cache/
  single-selection regression PASS. All four angles were exercised by semantic
  actions and NVS reboot checks. Separate 90° + Matrix test passed persistence;
  stable owner baseline restored to relative 0°, OEM theme and 80% brightness.
  These are automated MCU bench results; physical touch acceptance remains pending.
- Subsequent combined Wi-Fi/instrument theme firmware supersedes the above binary;
  see [final combined verification](wifi-and-instrument-themes-2026-10-01.md).

The owner was using the device during development: the before-flash runtime
snapshot had dashboard 3, while the loaded snapshot had dashboard 2. Other common
fields and brightness matched. This runtime snapshot difference does not provide
an exact pre/post-NVS equality assertion; migration preservation is established by
the canonical-wire test. Automated final bench checks use their own stable loaded
baseline and preserve it. The final test restores the current loaded orientation baseline;
manual brightness remains the owner's current 80%, not overwritten by the 70%
new-settings default.

## Owner-reported regressions and corrections

The first physical inspection identified reversed vertical touch selection at
90°, default LVGL "Text" on recreated navigation buttons, and an older multiple
Settings highlight issue exposed by reopening/changing themes. These reports
supersede the initial register-only rotation check.

- Quarter-turn touch conversion is now explicitly the inverse of the installed
  esp_lvgl_port logical-to-panel area mapping. 90° maps native (x,y) to
  (height−1−y,x); 270° maps to (y,width−1−x). Portable tests compare original
  corners and interior points to an independent panel-area mapping, as well as
  fitting/rotation inverse behavior. Initial forward/inverse round trips alone
  could not detect the wrong direction because both functions were consistent.
- Cached text skips assignment only if the actual label already borrows that
  cache buffer. A newly created label with unchanged cached content still receives
  its text, eliminating LVGL's default "Text" after aspect changes.
- Settings theme reset clears actual old accent borders/background/glow and
  invalidates selection cache. Opening Settings clears prior row decoration;
  update applies the current row and clears all others. Shared helpers ensure
  rendering and resets agree. Only the selected row receives the highlight.
- `ui-test` additionally prints a separate actual-LVGL regression result via
  `display_app.h`, `display_app.c`, `main.c` and `ui_screens.c`. Isolated unloaded
  objects verify text-cache reuse across new labels and eight selection/theme-reset
  cycles with exactly one bordered row and no unselected glow. The owner's scene,
  settings and history are never loaded/replaced by this regression test.

## Remaining checks and next steps

P0: owner visual/touch acceptance of all four angles. Purpose: verify actual panel
appearance, direction, text fit and physical hit alignment. Components: ST7789,
XPT2046 and portrait scenes. Acceptance: upright complete scene, readable values,
correct card/Settings selection, and correct swipe/edge action in each angle.
Needs physical inspection; mathematical/register tests do not substitute for it.

P1: optionally reduce aspect-change reconstruction latency using retained trees
or incremental geometry. Purpose: faster orientation switching. Components:
scene ownership and LVGL layout. Acceptance: no lost history/bindings and stable
heap after repeated rotations; risk is stale borrowed-label buffers or objects.
Current half-turn changes already retain objects. No vehicle input is required.

P2: existing real-car validation remains separate. Rotation does not alter CAN,
protocols, acquisition, warnings, ambient sensing or ignition behavior. CRT/fade
visual sign-off from the preceding delivery remains separately pending unless the
owner confirms it; orientation startup alone does not establish brightness quality.

Research: [LVGL display rotation](https://lvgl.io/docs/open/9.4/details/main-modules/display/rotation.html)
and inspected local `espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_disp.c`
rotation-update callback. Original implementation uses existing SDK/LVGL APIs;
no third-party artwork, font or code bundle was copied. Scratch evidence is in
this chat's `work/rotation-*.log`, `rotation-result.json` and Web fixture.
The tested image contains the pre-commit version string; committing the source
and documentation does not replace that tested binary.
