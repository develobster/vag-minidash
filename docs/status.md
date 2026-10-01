# Current feature and evidence matrix

Focus update (2026-10-01): Volkswagen/Porsche now show a value-driven needle
only in single-signal Focus; ordinary cards have no instrument arcs. Five
reference vehicle profiles, Tiguan fallback and Auto/manual ranges use schema 3.
Automatic matching is connected through host discovery → USB, not autonomous
MCU CAN acquisition. See [implementation, sources and acceptance](ui/focus-instruments-2026-10-01.md).

Wi-Fi/theme update (2026-10-01): local settings remain editable with AP enabled;
Wi-Fi shows an ON/OFF indicator. Eleven themes now include Volkswagen/Porsche,
monospaced retro fonts and distinct frames/instrument artwork. See
[implementation, tests and remaining checks](ui/wifi-and-instrument-themes-2026-10-01.md).

Rotation update (2026-09-30): Settings supports 0/90/180/270 degrees, with
portrait card/grid reflow and transformed calibrated touch. Configuration schema
2 migrates old build-only orientation to relative 0 without changing common
settings. See [implementation, migration and verification](ui/rotation-and-portrait-2026-09-30.md).

Theme/startup update (2026-09-30): nine presets including Terminal, Matrix,
Fallout, Amber CRT, Cyberpunk and Arctic; boot-only CRT reveal with autonomous
LEDC fade (~2 s) to saved brightness; new-settings default is now 70%.
See [implementation and acceptance](ui/themes-and-crt-startup-2026-09-30.md).

Current backlight update (2026-09-30): GPIO4 is owner-confirmed and enabled for
LEDC PWM (5 kHz, 12 bit, gamma 2.0), using the existing Brightness setting and
NVS. Manual Settings control, 10% readable minimum, reboot persistence and cold
USB boot have passed physical bench acceptance. Prior FIXED/3V3 descriptions
below record the preceding wiring. See [backlight implementation and acceptance](hardware/backlight-pwm-2026-09-30.md).


Audited 2026-09-30 against integration base `22a346b`. See the
[consolidation report](research/repository-consolidation-2026-09-30.md) for fresh
acceptance results and actual delivery state. Historical hardware results below
come from the UI delivery `6f436dd`; no new device/car test is claimed.

The worktree subsequently synced cleanly to consolidation `3e5c6ca`; see the
[real-car preparation report](validation/real-car-preparation-2026-09-30.md)
for new strict recording/replay tools, tests and driver inspection. The current
[first-session runbook](validation/first-real-car-session.md) reports **BLOCKED**:
VW D-PDU x64 DLLs are installed, J2534 FunctionLibrary is not evidenced, Windows
VNCI passive capture and exact identity probe layouts remain unavailable.

Labels are independent: **IMPLEMENTED** describes code; **OFFLINE TESTED** is a
synthetic/native contract; **HARDWARE TESTED** is a recorded bench delivery;
**REAL-CAR VALIDATED** requires exact vehicle/ECU/source evidence. **PARTIAL**,
**RESEARCH ONLY**, **NOT IMPLEMENTED** identify incomplete product paths.
Registry confidence metadata alone does not grant a validation label.

| Component | Status / evidence | Code and tests | Limits |
|---|---|---|---|
| Display | IMPLEMENTED; HARDWARE TESTED for ST7789 backend | `firmware/main/st7789_backend.c`, `display_manager.c`, `ui_screens.c`; UI delivery record | 320x240 baseline with 0/90/180/270 user rotation; LCD marking unconfirmed; ILI9341 backend exists but is not the validated current panel |
| XPT2046 touch | IMPLEMENTED; HARDWARE TESTED | `xpt2046_touch.c`, `ui_touch_map.c`, `ui_touch_nvs.c`; `core/tests.c`, device UI record | Five-point affine/NVS; calibration independent of UI config; physical edge/readability checks remain |
| Multiple screens | IMPLEMENTED; OFFLINE/HARDWARE TESTED | `ui_core.c`, `ui_screens.c`; native/device self-tests | Telemetry source absent on MCU |
| Layouts / four Custom pages | IMPLEMENTED; OFFLINE TESTED; prior device contracts | `ui_configuration.c`, `ui_model.c`, `ui_screens.c`; `core/tests.c`, `core/test_json.c` | 1/2/4/6/8 and mixed layouts; dense readability not physically accepted |
| Themes / day-night / localization | IMPLEMENTED; OFFLINE/HARDWARE TESTED contracts | `ui_theme.c`, `ui_i18n.c`, `ui_font_ru_*.c`; native/device glyph checks | English/Russian; physical labels and dim/night choices require visual acceptance |
| Theme editor | IMPLEMENTED; OFFLINE TESTED; prior AP HTTP contracts | `webui.html`, `ui_configuration_json.c`; `core/test_webui.cjs`, `ui_web.c` self-tests | Browser/phone rendering and sustained FPS not measured |
| Screen navigation / on-screen controls | IMPLEMENTED; OFFLINE/HARDWARE TESTED | `ui_core.c`, `ui_screens.c`, `display_app.c`; native/device self-tests | Semantic actions; Settings select/CHANGE separated; all dashboard card pickers |
| GPIO buttons / steering-stalk input | PARTIAL; OFFLINE TESTED abstraction | `ui_platform.c`, `ui_platform_selftest.c` | Debounce/long press/repeat/lockout model; no wired GPIO adapter or evidence-backed CAN/LIN mapping |
| BLE / phone app | PARTIAL bring-up; NOT IMPLEMENTED product app | `firmware/main/main.c` bring-up | ESP32-S3 BLE, no Classic SPP or product configuration protocol |
| Wi-Fi | IMPLEMENTED; prior HARDWARE TESTED AP contracts | `ui_web.c`, `main.c`; prior four AP cycles | Explicit temporary AP; no always-on remote service |
| Web UI | IMPLEMENTED; OFFLINE TESTED; prior HARDWARE TESTED HTTP | `ui_web.c`, `webui.html`; native JSON/Node tests | Random WPA2/session token/revision/bounds; driving gate lacks live speed provider; local HTTP plaintext |
| Device settings | IMPLEMENTED; OFFLINE/HARDWARE TESTED persistence | `ui_configuration*.c`, `ui_settings_nvs.c`; JSON/native/device tests | Schema 2 CRC, legacy migration, separate touch key; four display orientations |
| Backlight/manual PWM | IMPLEMENTED; OWNER HARDWARE TESTED | `backlight_controller.c`, `display_manager.c`; backlight delivery report | GPIO4 LEDC 5 kHz/12 bit, 10–100%, sleep-off, saved boot fade; AUTO has no ambient source |
| Passive CAN acquisition | IMPLEMENTED host; OFFLINE TESTED; PARTIAL standalone | `passive.py`, `dbc.py`, `pipeline.py`, `can_capture.py`; CAN/registry/MQB/pipeline tests | MCU CAN disabled; real connection-point visibility and hardware listen-only unverified |
| UDS polling | IMPLEMENTED host; OFFLINE TESTED | `uds.py`, `runtime.py`, `pipeline.py`; runtime/live/pipeline tests | No physical MiniDash VNCI acceptance; extended session opt-in; missing bus feedback/adaptive UI priority |
| Transport / J2534 / VNCI | PARTIAL; OFFLINE TESTED ABI/lifecycle | `transports.py`, `j2534.py`; `test_live.py`, `test_transport.py` | Driver inventory/evidenced routes exist; MiniDash physical VCI route not proven |
| ISO-TP | IMPLEMENTED host; OFFLINE TESTED multiframe/VirtualBus | `transports.py`, `transport_lab.py`; live/transport/pipeline tests | Linux SocketCAN capture test skips on Windows; MCU implementation absent |
| ECU discovery | IMPLEMENTED configured endpoints; OFFLINE TESTED | `discovery.py`, `__main__.py`; `test_pipeline.py`, `test_catalog.py` | Not blind gateway/network scanning; identity reads do not validate signal candidates |
| VehicleIdentity | IMPLEMENTED; OFFLINE TESTED | `identity.py`, `discovery.py`; vehicle-knowledge/pipeline/evidence tests | Snapshot provenance/connection point must be recorded physically |
| EcuIdentity / IdentityContext | IMPLEMENTED; OFFLINE TESTED | `identity.py`, `knowledge.py`, `discovery.py`; evidence/knowledge/pipeline tests | Exact fingerprints gate codecs and reconnect checks |
| Knowledge providers / provenance | IMPLEMENTED; OFFLINE TESTED | `knowledge.py`, `vag.py`, `registry.py`; `test_vehicle_knowledge.py` | Platform-agnostic core; canonical/raw mapping explicit; license/evidence retained |
| MQB / VAG knowledge | PARTIAL; OFFLINE TESTED; candidates require car evidence | `data/external/`, `vag.py`; `test_mqb_knowledge.py` | 134 definitions (53 CAN/81 UDS) are not 134 validated telemetry signals |
| Exact Engine coolant F405 | REAL-CAR VALIDATED codec for documented exact identity | `data/ecu_profiles/j623-czpa-5na907115d.json`; `test_real_vehicle_evidence.py` | ODIS/physical trace evidence, not proof of MiniDash J2534 live path; no other ECU/platform extrapolation |
| Signal registry | IMPLEMENTED; OFFLINE TESTED | `registry.py`, `profiles.py`; registry/scalar/knowledge tests | Conflict/schema/codec validation; candidate labels remain candidates |
| MCD import | PARTIAL; OFFLINE TESTED FLOAT32/TEXTTABLE | `mcd_import.py`, `profiles.py`; `test_scalar_codecs.py`, typed fixture | Single byte-aligned leaf only; bounded enum/no defaults, no multi-field/mux/general formulas |
| Acquisition planner / scheduler | IMPLEMENTED; OFFLINE TESTED; PARTIAL product budget | `pipeline.py`, `runtime.py`; MQB/runtime/pipeline tests | Passive grace/fallback/cooldown/backoff; no measured bus-load budget or active-screen priority |
| SignalStore | IMPLEMENTED; OFFLINE TESTED | `runtime.py`; runtime/registry/pipeline/replay tests | Quality/TTL and source provenance; host store distinct from embedded presentation store |
| SourceArbitrator | IMPLEMENTED; OFFLINE TESTED | `arbitration.py`, `pipeline.py`; knowledge/pipeline/replay tests | Offline replay does not promote evidence or publish a live source |
| Replay | IMPLEMENTED; OFFLINE TESTED | `replay.py`, `CanTraceReader`, CLI; `test_can_replay.py`, `test_real_car_validation.py` | CAN uses existing decoder/store/arbitration; recorded UDS also uses production discovery/planner/scheduler with offline Transport; no DLL/bus I/O; private complete trace required |
| DTC read | NOT IMPLEMENTED live product; OFFLINE TESTED simulator | `diagnostics.py`, `transport_lab.py`; `test_diagnostics.py` | FakeECU lab only; no host production/MCU DTC inventory implementation |
| DTC clear | NOT IMPLEMENTED live product | Simulator paths only | No 0x14 production workflow/confirmation/vehicle guard acceptance |
| Coding/adaptation | RESEARCH ONLY / simulator groundwork | `core.py`, legacy profiles/journal tests | No live allowlist/write service; no Web endpoint |
| CLI / doctor / trace | IMPLEMENTED; OFFLINE TESTED | `__main__.py`, `discovery.py`, `api.py`; live/preflight/probe/pipeline tests | Doctor does not open VCI; discover/signals/watch physical acceptance still needed |
| Logging | IMPLEMENTED host; NOT IMPLEMENTED MCU recording | `__main__.py`, `can_capture.py`; CAN/probe/replay tests | JSONL metadata/errors/freshness; storage/SD backend and safe record-state UX absent on MCU |
| Real-car evidence workflow | IMPLEMENTED private one-shot UDS trace/preflight/markers/replay/reference tools; OFFLINE TESTED; BLOCKED physical VNCI | `validation.py`, `validation_session.py`, `tools/real_car_*.py`, `docs/validation/*`; new evidence tests | D-PDU and passive VNCI adapters missing; ECU identity layouts and synchronized physical captures needed; no auto promotion to validated |
| ESP32 firmware | IMPLEMENTED UI/bring-up; prior HARDWARE TESTED | `firmware/main/`, IDF CMake/defaults/lock; UI/device record | Host USB bridge only optional source; not autonomous car monitoring yet |
| Display abstraction / Lite | PARTIAL; OFFLINE TESTED shared core | `ui_platform.c`, `ui_model.c`, `core/CMakeLists.txt`; portable tests | No STM32 linked firmware, OLED driver/board/pins or complete RAM/flash fit evidence |

## Priorities

**P0:** consolidation acceptance/delivery and local sync completed; preserve local
artifacts; bench accept RU, page wrap, picker/settings/persistence; establish
MiniDash physical read-only VNCI/identity acceptance and passive capture visibility.

**P1:** measured poll/bus budget and screen subscriptions; protected standalone
CAN/power integration; receive/store/render watchdog and logging resources; broaden
telemetry only with exact identity/reference evidence. DTC read comes before a
separate reviewed, guarded DTC-clear workflow.

**P2:** controlled adaptation/coding allowlists and recovery, verified car-button
adapter, AUTO illumination source/PWM hardware, optional BLE app/OTA authenticity,
confirmed Lite target. Hardware capability bits alone do not implement these paths.
