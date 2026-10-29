# CYD integration USB bench update — 29 October 2026

The owner explicitly authorized publishing the integration branch and flashing
the connected device. The source used was `3b29c71`, also contained in the
published `78315bf` (which adds only workflow/docs). The separate
`integration/cyd-unified` branch was pushed without merging into the main branch.
The [validation run](https://github.com/develobster/vag-minidash/actions/runs/40783829121)
passed host and all five firmware matrix jobs, including both CYD variants.

## Before writing

- COM6 identified the connected device as ESP32-D0WD-V3, 4 MB flash; the
  previously installed application descriptor was `cyd-0.5.1-fast`, IDF 5.5.5.
- A full 4,194,304-byte flash backup was read, verified against flash, hashed
  and copied locally. Both copies and the hash stay in the ignored private
  directory `.local/private-cyd-20261029/`; they contain private information.
- Flash encryption count was zero and secure boot flags were off. eFuses were
  read only; none were changed. The existing partition table exactly matched
  the new table, including NVS at `0x9000` and factory app at `0x10000`.
- The installed bootloader and the new build have the same IDF and bootloader
  version; binary differences were confined to build timestamp and checksum/hash.
  The bootloader and partition table were not rewritten.

## Write and first boot

The **CYD listen-only** application (`CONFIG_MINIDASH_STANDARD_OBD` disabled)
was written at `0x10000`, 2,024,096 bytes. The write hash and an independent
`verify_flash` both passed. Before first boot, NVS `0x9000..0xefff` was read
and compared byte-for-byte to the full backup: identical. No erase-flash or
eFuse write was performed. Flashing the app necessarily erased its own app
sectors; it did not write the NVS or partition-table sectors.

The installed application reported display and touch ready, existing touch
calibration present, DEMO off, CAN ready in listen-only, TX unarmed, RX=0,
TX=0 and UDS off. The on-device UI self-test reported 27 passes, 0 failures,
and the view self-test passed. Serial commands selected all five automotive
screens and returned to the original home screen; this confirms software scene
creation, not visual inspection or actual touch accuracy. Switching DEMO on and
back off produced flagged synthetic values in DEMO, then unavailable real values
with demo flags cleared; TX remained unarmed. No CAN or diagnostic request was
sent. The post-boot exported configuration, excluding revision, was equal again
after navigation and mode tests.

Manual Wi-Fi AP start and stop in DEMO were exercised and the device returned
to DEMO off, Wi-Fi off, home screen, with the exported configuration restored.
An initial attempt addressed the wrong menu row and temporarily changed the
dashboard; it was reversed and the full exported configuration compared equal.
The first AP stop attempt returned to the dashboard instead of stopping the AP;
the AP was then explicitly stopped from Settings and status confirmed Wi-Fi off.
The mutable built-in HTTP self-test was not run: it changes and saves settings,
and preserving the user's settings was prioritized. Browser tests against a mock
passed earlier; actual Web HTTP and Wi-Fi with live CAN traffic remain untested.

Internal free heap was 89,968 bytes shortly after first boot, with minimum
89,128 and largest block 86,016. Following screen and AP tests, reported heap
was 79,812 bytes, observed minimum 28,668 and largest block 32,768; after ten
seconds idle those values remained unchanged. After a software reboot, the
exported configuration again matched the post-install snapshot; once the UI
was ready, free heap was 89,940, minimum 58,392 and largest block 57,344 bytes.
Two more AP start/stop cycles reported free heap of 79,776 and 79,780 bytes
after stopping; the minimum in that run was 32,576 and largest free block after
stopping was 32,768 bytes. No progressive decrease was observed across these two
cycles. DEMO, Wi-Fi and the exported settings were restored afterwards. These
observations neither exclude a leak nor establish an acceptable operating limit.
Investigate longer AP cycling, fragmentation and HTTP workload before any vehicle use. The vehicle task
reported stack high-water mark 6,100 in the API's reported units; validate all
task stacks under combined workload before acceptance.

The connected device is left at home screen, Wi-Fi off, DEMO off, listen-only,
TX unarmed. No vehicle or CAN generator was connected or exercised by this task;
the physical wiring state is not inferred from zero RX. Actual touch operation,
display appearance, diagnostic transmission, bus-off recovery, sustained CAN
loss and vehicle signals are unverified. Do not declare the system ready for
vehicle operation or claim sleep current or measurement accuracy.

## Drag touch freeze reported after installation

The owner reported a freeze while switching screens at Drag. USB status remained
responsive when inspected and reported screen 4; no crash log was observed.
Code inspection found that a touch press inside Drag's RESET hit box could return
while still holding the LVGL lock. This can block subsequent rendering and touch
handling. The handler now releases the lock before returning. The report does
not establish that the RESET hit box was touched, so this is a plausible cause,
not a confirmed reproduction. The updated CYD listen-only image was built and installed over USB. A fresh
full-flash backup was read and verified before the app-only write; the app image
was verified afterward and the NVS partition matched the backup before reboot.
USB status after reboot reported display, touch, and calibration ready, demo and
Wi-Fi off, and TX unarmed. CYD and S3 builds, 28 core tests, and two rendering
tests passed. The physical touch path and reported freeze remain unverified.


## Drag RESET and screen-switch latency follow-up

The owner reported that pressing RESET on Drag switched to the next screen,
and that the older pages draw markedly more slowly than the automotive pages.
The press handler previously queued a reset but the release still entered
right-edge navigation. The press and release coordinates now share a scaled
hit test, and a press that starts in RESET consumes the release; a reset is
queued only if released within the hit area. Other edge navigation is unchanged.
The UI core self-test checks hit area boundaries. Actual finger input and timer
reset on the physical device require owner confirmation.

On the attached CYD, the diagnostic first-render timing measured the five
older pages at 613, 611, 606, 554 and 774 ms, versus 95–148 ms for sampled
automotive pages. The older scenes are reconstructed on this SRAM-constrained
target, and their populated views are more expensive to render. An experimental
CYD change disabling rounded cards and gradients measured 670, 666, 663, 608
and 817 ms for the same pages, so it was removed; no performance improvement is
claimed. The measurement includes scene build and first LVGL render, not a
photographic measurement of panel completion. Next investigate LVGL draw and
SPI flush profiling per scene and theme, retaining existing design and memory
limits; accept a change only after repeatable on-device improvement and visual
review, including low-memory operation and touch responsiveness.

ESP-IDF 5.5.5 CYD listen-only and S3 builds succeeded, as did 28 core tests,
two automotive LVGL render tests, and 27 on-device UI tests plus the view
self-test. The flash was backed up and verified before installation. NVS was
checked separately; private backups remain local and must not be committed.
No vehicle test or physical touch confirmation was performed.


## Larger Drag RESET and lightweight CYD data pages

The owner reported difficulty hitting RESET and requested older pages be
redesigned for speed like the automotive pages. On the 320 x 240 layout the
scaled touch target now covers x=220..319 and y=94..145 (previously x=240..319
and y=108..131). Press and release must both be within that target; a press
starting there consumes the release, leaving navigation on the rest of the edge.
A visible outlined RESET control is added and the Drag progress bar is shortened
to avoid it. The control must be checked physically, including edges and swipes.

On CYD, dashboard, custom dashboard, performance, temperatures, drivetrain and
diagnostics fields now use flat transparent containers and suppress decorative
frame and pixel-background drawing. Their current field bindings, status labels,
editable signal slots, custom widget types, touch regions, colors and update
logic are retained. Focus, settings and other non-data screens retain their
existing presentation. S3 retains its existing data page styling. This visibly
changes themed backgrounds and decorative frames on the CYD data pages.

On the same connected CYD and settings, first-render diagnostics reported pages
0..4 at 320, 314, 311, 275 and 530 ms, versus the prior 613, 611, 606, 554 and
774 ms. Night and Drag reported 97 and 144 ms respectively. Timing includes
scene construction and first LVGL rendering; it is not an optical measurement
of the completed LCD refresh. Diagnostics remains slower, and theme-dependent
performance and prolonged operation require further profiling and physical
review. Next: verify readability, themes, widget editing, graph/bar updates,
and expanded RESET on hardware; then investigate the diagnostics scene's 265 ms
build time and measure vehicle data and memory under realistic operation.

ESP-IDF 5.5.5 CYD listen-only and S3 builds passed, as did 28 core tests, 28
native UI tests, 27 on-device UI tests and the on-device view self-test. Before
the app-only USB write, a full flash backup was captured and verified; the app
write hash matched and the NVS partition was byte-identical before first boot.
Post-boot status reported display and touch ready and calibrated, with TX
unarmed. Actual touch operation, appearance on the physical panel, vehicle
signals and all configurations remain unverified. Backups are private and local.


## Russian language on automotive screens

The owner reported that the five automotive screens stayed English while
Russian was selected. Automotive screens now receive the existing language
setting on show/update and translate their titles, field labels, navigation
hint, RESET, source/demo captions, regeneration state, Drag state and reason.
Platform identifiers and standard technical units remain unchanged where
appropriate. Static captions and dynamic text with Cyrillic use the bundled
Russian fonts; captions choose a smaller supported size when needed. English
remains available and changing language refreshes existing scenes. This affects
CYD and S3 without a settings format change.

Native LVGL tests switch English to Russian and back on all five screens, in
320x240 and 240x320, check Cyrillic font availability and text width, render
images, and retain the existing unchanged/partial-refresh checks. Both CYD and
S3 firmware builds and 28 native UI tests passed with ESP-IDF 5.5.5. All five
Russian landscape software-render images were visually inspected. On the connected
CYD, the full flash backup was verified, only the app partition was written,
the write hash matched, and NVS remained byte-identical before boot. Post-boot
status reported calibrated touch and demo enabled (preserved user setting);
27 on-device UI checks and the view test passed, all five screens opened, and
the saved language remained Russian.
Physical text readability, user touch and all live vehicle-data states still
need owner review on the panel. Next verify those states and Cyrillic in every
selected theme; use the private local flash backup for recovery if needed.
