# Current feature and evidence matrix

Updated 2026-10-03, standalone branch on consolidation `3e5c6ca`.
See [current report](research/standalone-final-2026-10-03.md) and
[first autonomous car test](../REAL_CAR_TEST.md). Historical UI hardware results
come from `6f436dd`; no new device/car test is claimed. Python modules below are
preserved development/research tools, not dependencies of standalone operation.

Labels are independent: **IMPLEMENTED** describes code; **OFFLINE TESTED** is a
synthetic/native contract; **HARDWARE TESTED** is a recorded bench delivery;
**REAL-CAR VALIDATED** requires exact vehicle/ECU/source evidence. **PARTIAL**,
**RESEARCH ONLY**, **NOT IMPLEMENTED** identify incomplete product paths.
Registry confidence metadata alone does not grant a validation label.

| Component | Status / evidence | Code and tests | Limits |
|---|---|---|---|
| Display | IMPLEMENTED; HARDWARE TESTED for ST7789 backend | `firmware/main/st7789_backend.c`, `display_manager.c`, `ui_screens.c`; UI delivery record | 320x240 rotation 2; LCD marking unconfirmed; ILI9341 backend exists but is not the validated current panel |
| XPT2046 touch | IMPLEMENTED; HARDWARE TESTED | `xpt2046_touch.c`, `ui_touch_map.c`, `ui_touch_nvs.c`; `core/tests.c`, device UI record | Five-point affine/NVS; calibration independent of UI config; physical edge/readability checks remain |
| Multiple screens | IMPLEMENTED; OFFLINE/HARDWARE TESTED UI | `ui_core.c`, `ui_screens.c`, `acquisition.c` | Native OBD source added; acquisition not car-validated |
| Layouts / four Custom pages | IMPLEMENTED; OFFLINE TESTED; prior device contracts | `ui_configuration.c`, `ui_model.c`, `ui_screens.c`; `core/tests.c`, `core/test_json.c` | 1/2/4/6/8 and mixed layouts; dense readability not physically accepted |
| Themes / day-night / localization | IMPLEMENTED; OFFLINE/HARDWARE TESTED contracts | `ui_theme.c`, `ui_i18n.c`, `ui_font_ru_*.c`; native/device glyph checks | English/Russian; physical labels and dim/night choices require visual acceptance |
| Theme editor | IMPLEMENTED; OFFLINE TESTED; prior AP HTTP contracts | `webui.html`, `ui_configuration_json.c`; `core/test_webui.cjs`, `ui_web.c` self-tests | Browser/phone rendering and sustained FPS not measured |
| Screen navigation / on-screen controls | IMPLEMENTED; OFFLINE/HARDWARE TESTED | `ui_core.c`, `ui_screens.c`, `display_app.c`; native/device self-tests | Semantic actions; Settings select/CHANGE separated; all dashboard card pickers |
| GPIO buttons / steering-stalk input | PARTIAL; OFFLINE TESTED abstraction | `ui_platform.c`, `ui_platform_selftest.c` | Debounce/long press/repeat/lockout model; no wired GPIO adapter or evidence-backed CAN/LIN mapping |
| BLE / phone app | PARTIAL bring-up; NOT IMPLEMENTED product app | `firmware/main/main.c` bring-up | ESP32-S3 BLE, no Classic SPP or product configuration protocol |
| Wi-Fi | IMPLEMENTED; prior HARDWARE TESTED AP contracts | `ui_web.c`, `main.c`; prior four AP cycles | Explicit temporary AP; no always-on remote service |
| Web UI | IMPLEMENTED; OFFLINE TESTED; prior HARDWARE TESTED HTTP | `ui_web.c`, `webui.html`; native JSON/Node tests | WPA2/session/revision/bounds; native speed gate still needs physical validation; local HTTP plaintext |
| Device settings | IMPLEMENTED; OFFLINE/HARDWARE TESTED persistence | `ui_configuration*.c`, `ui_settings_nvs.c`; JSON/native/device tests | Schema 1 CRC, legacy migration, separate touch key; no arbitrary new rotations |
| Backlight/manual PWM | PARTIAL; OFFLINE TESTED controller | `ui_platform.c`, `display_manager.c` | Current BL is 3.3 V power, FIXED; hardware PWM/AUTO blocked; no confirmed vehicle illumination signal |
| Passive CAN acquisition | IMPLEMENTED host and gated native core; OFFLINE TESTED | `acquisition.c`, `passive.py`, `dbc.py`; core/MQB tests | No enabled native profile; OBD visibility/checksum/counters and hardware listen-only unverified |
| UDS polling | IMPLEMENTED host and gated native core; OFFLINE TESTED | `acquisition.c`, `uds.py`, `runtime.py` | No enabled native manufacturer profile; exact VIN/identity/endpoint/evidence required |
| Transport / J2534 / VNCI | PARTIAL; OFFLINE TESTED ABI/lifecycle | `transports.py`, `j2534.py`; `test_live.py`, `test_transport.py` | Driver inventory/evidenced routes exist; MiniDash physical VCI route not proven |
| ISO-TP | IMPLEMENTED host and native; OFFLINE TESTED | `diagnostic_transport.c`, `acquisition.c`, core tests/VirtualBus | Native RX128 bytes, single-frame requests only; header/full-length validation before FC |
| Native OBD discovery | IMPLEMENTED; OFFLINE TESTED | `acquisition.c`, `vehicle_app.c` | Explicit 11-bit/500-kbit preset; PID capabilities/VIN, not a VAG full-ECU scan |
| ECU discovery | IMPLEMENTED configured endpoints; OFFLINE TESTED | `discovery.py`, `__main__.py`; `test_pipeline.py`, `test_catalog.py` | Not blind gateway/network scanning; identity reads do not validate signal candidates |
| VehicleIdentity | IMPLEMENTED; OFFLINE TESTED | `identity.py`, `discovery.py`; vehicle-knowledge/pipeline/evidence tests | Snapshot provenance/connection point must be recorded physically |
| EcuIdentity / IdentityContext | IMPLEMENTED; OFFLINE TESTED | `identity.py`, `knowledge.py`, `discovery.py`; evidence/knowledge/pipeline tests | Exact fingerprints gate codecs and reconnect checks |
| Knowledge providers / provenance | IMPLEMENTED; OFFLINE TESTED | `knowledge.py`, `vag.py`, `registry.py`; `test_vehicle_knowledge.py` | Platform-agnostic core; canonical/raw mapping explicit; license/evidence retained |
| MQB / VAG knowledge | PARTIAL; OFFLINE TESTED; candidates require car evidence | `data/external/`, `vag.py`; `test_mqb_knowledge.py` | 134 definitions (53 CAN/81 UDS) are not 134 validated telemetry signals |
| Exact Engine coolant F405 | REAL-CAR VALIDATED codec for documented exact identity | `data/ecu_profiles/j623-czpa-5na907115d.json`; `test_real_vehicle_evidence.py` | ODIS/physical trace evidence, not proof of MiniDash J2534 live path; no other ECU/platform extrapolation |
| Signal registry | IMPLEMENTED; OFFLINE TESTED | `registry.py`, `profiles.py`; registry/scalar/knowledge tests | Conflict/schema/codec validation; candidate labels remain candidates |
| MCD import | PARTIAL; OFFLINE TESTED FLOAT32/TEXTTABLE | `mcd_import.py`, `profiles.py`; `test_scalar_codecs.py`, typed fixture | Single byte-aligned leaf only; bounded enum/no defaults, no multi-field/mux/general formulas |
| Acquisition planner / scheduler | IMPLEMENTED host/native; OFFLINE TESTED | `acquisition.c`, `pipeline.py`, `runtime.py` | Native visible/background priority, one active request, bounds; no measured physical bus-load budget |
| SignalStore | IMPLEMENTED; OFFLINE TESTED | `runtime.py`; runtime/registry/pipeline/replay tests | Quality/TTL and source provenance; host store distinct from embedded presentation store |
| SourceArbitrator | IMPLEMENTED; OFFLINE TESTED | `arbitration.py`, `pipeline.py`; knowledge/pipeline/replay tests | Offline replay does not promote evidence or publish a live source |
| Replay | IMPLEMENTED; OFFLINE TESTED | `replay.py`, `CanTraceReader`, CLI; `test_can_replay.py` | Existing decoder/store/arbitration; Classical CAN recorder JSONL, not BLF/ASC/FD; no transport/scheduler I/O |
| DTC read | IMPLEMENTED native Mode03; OFFLINE TESTED | `acquisition.c`, `display_app.c`, core/VirtualBus tests | Selected emissions ECU, fresh speed=0, max12/truncation; no descriptions/full inventory/0x19 |
| DTC clear | NOT IMPLEMENTED live product | Simulator paths only | No 0x14 production workflow/confirmation/vehicle guard acceptance |
| Coding/adaptation | RESEARCH ONLY / simulator groundwork | `core.py`, legacy profiles/journal tests | No live allowlist/write service; no Web endpoint |
| CLI / doctor / trace | IMPLEMENTED; OFFLINE TESTED | `__main__.py`, `discovery.py`, `api.py`; live/preflight/probe/pipeline tests | Doctor does not open VCI; discover/signals/watch physical acceptance still needed |
| Logging | IMPLEMENTED host; PARTIAL native RAM trace | `acquisition.c`, `md_vehicle_app_trace`, host recorder | Ring96 frames, C snapshot API; no HTTP export/persistent trip logger/SD backend |
| Real-car evidence workflow | IMPLEMENTED documents/manifest; PARTIAL execution | `docs/runtime/tiguan-mqb-*`, identity/provenance code; evidence tests | New MQB candidates, timing corrections, car buttons and illumination need synchronized physical captures |
| ESP32 firmware | IMPLEMENTED autonomous OBD/UI; OFFLINE TESTED | `firmware/main/`, core/acquisition harness | Native car path not physically tested; default CAN off, explicit presets; QUIET is not power-off |
| Display abstraction / Lite | PARTIAL; OFFLINE TESTED shared core | `ui_platform.c`, `ui_model.c`, `core/CMakeLists.txt`; portable tests | No STM32 linked firmware, OLED driver/board/pins or complete RAM/flash fit evidence |

## Priorities

**P0:** repeat firmware builds after toolchain recovery; protected power/transceiver
bench, physical UI and autonomous read-only Tiguan acceptance. No PC required at runtime.

**P1:** measured bus budget/heap/FPS, exact profile evidence/compiler, passive and
steering visibility, native ECU-scoped read-only diagnostics. DTC clear requires a
separate reviewed guard/confirmation workflow; it is not part of this release.

**P2:** controlled adaptation/coding allowlists and recovery, verified car-button
adapter, AUTO illumination source/PWM hardware, optional BLE app/OTA authenticity,
confirmed Lite target. Hardware capability bits alone do not implement these paths.
