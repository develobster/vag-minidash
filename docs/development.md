# Разработка и повторяемые проверки

Текущая ветка: **`integration/standalone`**. См.
[интеграцию экранных исправлений и свежие проверки от 14 октября](development/standalone-realignment-2026-10-14.md),
[карту веток](development/branch-consolidation.md),
[поставку и регрессии](development/standalone-consolidation-2026-10-05.md),
[конфигуратор на ПК](web-configurator.md),
[режим проверки автомобиля](validation/car-validation-mode.md) и
[русский интерфейс/стендовую прошивку](development/russian-ui-flash-2026-10-05.md).
Обычная сборка прошивки теперь проверяет собственные кадры `display_app.c`
до 3072 байт и известные вложенные пути UI с резервом 1024 байта при стеке
8192 байта. Исходный код `751d2f8` должен отклоняться скриптом
`python tools/check_ui_stack_regression.py --build-dir <путь-сборки>`.
Для UART0 доступен последний overlay `sdkconfig.uart-console`; при смене
профиля консоли нужен согласованный загрузчик по [инструкции](hardware/manual-flashing-ru.md).
The inherited merge `6ae2ed4` preserves
standalone `ab1c09c` and diverged real-car/UI `bf7e8ba`. See the
[analytics delivery](research/engine-analytics-delivery-2026-10-04.md) and
[branch audit](research/analytics-integration-2026-10-04.md). Older dated sections
below retain their historical context. Current configuration is schema 4.

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


Work in the existing repository/worktree. Read `AGENTS.md` and `git status`
before editing. Continue on `integration/standalone`; preserve other worktree
changes. The proven base is `751d2f8` plus the seven local Web Configurator commits
ending at `bdd5f7a`, combined by normal merge. Older consolidation reports retain
their historical branch state; they are not the current development baseline.

## Repository map

| Path | Ownership / entry point |
|---|---|
| `minidash_live/` | Preserved Python development/research diagnostic runtime, CLI (`python -m minidash_live`); not needed by standalone firmware |
| `data/ecu_profiles`, `data/vehicles`, `data/external` | Reviewed identity/vehicle/provider knowledge and candidate provenance |
| `profiles/` | Existing legacy and diagnostic profile formats; not interchangeable with all registry files |
| `firmware/` | ESP-IDF ESP32-S3 application; `main/` includes model, renderer, web page and hardware adapters |
| `firmware/main/webui.html`, `ui_web.c` | Device-served local editor and validation evidence API |
| `minidash_live/web/`, `package.json` | Mobile host configurator; Node is development/test tooling only |
| `core/` | Native CMake tests against physical shared firmware sources; JSON is optional in CMake but required by full editor acceptance |
| `tests/`, `tests/fixtures/` | Python contracts, VirtualBus/ISO-TP/DBC integration, reviewed synthetic fixtures |
| `tools/` | Existing bring-up/VCI scripts and offline acceptance runner |
| `can_capture.py` | Linux receive-only recorder and bounded recorded-trace reader |
| `core.py`, `diagnostics.py`, `app.py`, `transport_lab.py` | Retained desktop simulator/lab/history; do not replace live runtime with these paths |
| `docs/`, `artifacts/` | Versioned docs versus local private artifacts; see policy |

Sources are not moved. Imports, includes, CMake source paths, IDF assets and APIs
remain stable. Host settings schema v2 is independent of firmware schema 4.
Offline acceptance neither flashes hardware nor performs vehicle operations.

## Python and offline CLI

Use a local virtual environment with the pinned optional integrations:

```sh
python -m venv .venv
# Activate .venv using the shell's normal activation command.
python -m pip install -r requirements-transport.txt -r requirements-data.txt
python -m unittest discover -s tests -v
python -m minidash_live --help
python -m minidash_live discover --help
python -m minidash_live signals --help
python -m minidash_live watch --help
python -m minidash_live replay-can --help
python -m minidash_live doctor --config config/examples/tiguan-vnci-engine-read-only.template.json
```

The committed connection template is intentionally incomplete for a new machine;
doctor rejection without the evidenced DLL/config is expected, not a vehicle test.
`tests/test_pipeline.py` runs real VirtualBus/ISO-TP discovery, signals/watch and
fallback contracts. `tests/test_can_replay.py` runs CLI replay through the existing
engine with no physical bus/session. Do not run live discovery/watch without
the separate [first-car gate](runtime/tiguan-first-live.md).

The 22a346b baseline is 159 tests with one Windows AF_CAN skip when optional
packages are installed. A passing suite with many dependency skips is weaker
coverage; report exact counts. MCD FLOAT32/TEXTTABLE tests are included.

## Native C and editor logic

Use an installed C11 compiler, CMake and a compatible build tool. cJSON must come
from a known local source (the installed ESP-IDF has `components/json/cJSON`).
Do not silently omit JSON tests and call the result full acceptance.

```sh
cmake -S core -B core/build-acceptance -DMD_JSON_SOURCE_DIR=/absolute/path/to/cJSON
cmake --build core/build-acceptance
ctest --test-dir core/build-acceptance --output-on-failure --no-tests=error
python tools/verify_repository.py --native-build-dir core/build-acceptance --output-dir .local/acceptance/run-001
```

On Windows select an installed toolchain/generator explicitly if the default is
unavailable. A Zig compiler uses `-DCMAKE_C_COMPILER=/path/to/zig.exe`,
`-DCMAKE_C_COMPILER_ARG1=cc`, and its target flags; choose a compatible AR/RANLIB
and Ninja path. These are machine inputs, not hardcoded repository dependencies.

For restricted Windows processes, keep compiler caches and temporary files in
an ignored writable folder. This task used a scoped Zig 0.13.0 installation,
with no global compiler/runtime package upgrade:

```powershell
# From repository root, with the intended Python test environment active:
python -m pip install --target .local/native-tools --no-deps ziglang==0.13.0
$miniDashRoot = (Get-Location).Path
New-Item -ItemType Directory -Force -Path .local/compiler-temp | Out-Null
$env:TMP = "$miniDashRoot/.local/compiler-temp"
$env:TEMP = $env:TMP
$env:ZIG_GLOBAL_CACHE_DIR = "$miniDashRoot/.local/zig-global"
$env:ZIG_LOCAL_CACHE_DIR = "$miniDashRoot/.local/zig-local"
cmake -S core -B core/build-acceptance -G Ninja "-DCMAKE_C_COMPILER=$miniDashRoot/.local/native-tools/ziglang/zig.exe" '-DCMAKE_C_COMPILER_ARG1=cc' '-DCMAKE_C_FLAGS=-target x86_64-windows-gnu' '-DCMAKE_MAKE_PROGRAM=C:/Espressif/tools/ninja/1.12.1/ninja.exe' '-DCMAKE_AR=C:/Espressif/tools/esp-clang/esp-19.1.2_20250312/esp-clang/bin/llvm-ar.exe' '-DCMAKE_RANLIB=C:/Espressif/tools/esp-clang/esp-19.1.2_20250312/esp-clang/bin/llvm-ranlib.exe' '-DMD_JSON_SOURCE_DIR=C:/Espressif/frameworks/esp-idf-v5.5.5/v5.5.5/esp-idf/components/json/cJSON'
```

Adjust executable/SDK paths for the machine and retain these cache/temp variables
when running acceptance in the shell. No compiler binary or cache is committed.

The runner executes Python, rebuilds native targets, verifies all eight CTest groups,
generates a fresh Web fixture from `minidash-json-tests`, then runs
`node core/test_webui.cjs`. Logs and `summary.json` go to a new directory;
existing output is rejected. Missing optional dependencies, native build/test
or editor checks produce a nonzero/incomplete result. `--cmake`, `--ctest`,
`--node` accept installed executable paths. Firmware is a separate gate below.
Node tests cover editor logic with a fake DOM, not browser rendering or a phone.

The required `standalone_virtualbus` gate runs the actual firmware C acquisition
engine through `core/acquisition_harness.c`, using python-can VirtualBus and a
synthetic ECU. It checks VIN/DTC multi-frame responses, loss of ECU, no TX in
QUIET, bus fault/reconnect and exact deterministic replay in a fresh process.
No hardware interface or real vehicle trace is used. An absent harness is a
failure, not a skipped success. To run it separately:

```sh
python tools/verify_standalone.py --harness core/build-acceptance/minidash-acquisition-harness
```

Portable memory-safety check (GCC/Clang, repository root):

```sh
mkdir -p artifacts/local
gcc -std=c11 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I firmware/main core/test_acquisition.c firmware/main/acquisition.c firmware/main/diagnostic_transport.c -o artifacts/local/acquisition-sanitized
ASAN_OPTIONS=detect_leaks=0 artifacts/local/acquisition-sanitized
```

The leak detector is disabled because this execution environment previously
blocked task metadata access. This is not a firmware heap/FreeRTOS test; the
portable acquisition core itself does not dynamically allocate memory.

## ESP32-S3 configure/build

Use the installed ESP-IDF 5.5.5 environment and dependency manager. Preserve the
existing local sdkconfig/NVS/factory backup; do not repartition or flash as part
of repository acceptance. New clean builds use tracked `sdkconfig.defaults`,
`dependencies.lock`, `partitions.csv` and component declarations.

```sh
# In the initialized ESP-IDF environment, working directory firmware:
idf.py -B build-acceptance reconfigure
idf.py -B build-acceptance build
```

Windows installed environment example (from firmware):

```powershell
& 'C:/Espressif/tools/Microsoft.v5.5.5.PowerShell_profile.ps1'
$env:PYTHONUTF8 = '1'
$env:IDF_COMPONENT_MANAGER = '1'
$env:IDF_CCACHE_ENABLE = '0'
$miniDashFirmwareRoot = (Get-Location).Path
New-Item -ItemType Directory -Force -Path .local/compiler-temp | Out-Null
$env:TMP = "$miniDashFirmwareRoot/.local/compiler-temp"
$env:TEMP = $env:TMP
# Direct Python invocation also works when the profile's shell function is absent.
& "$env:IDF_PYTHON_ENV_PATH/Scripts/python.exe" "$env:IDF_PATH/tools/idf.py" -B build-acceptance -DCCACHE_ENABLE=0 reconfigure
& "$env:IDF_PYTHON_ENV_PATH/Scripts/python.exe" "$env:IDF_PATH/tools/idf.py" -B build-acceptance build
```

A successful historical image does not prove a fresh configure/build passed.
Report SDK ownership/permissions/component/network failures separately. No
acceptance helper opens serial ports, CAN/VCI, executes a flash, or changes efuses.

## Bench/car acceptance next

### Explicit standalone build presets

From `firmware/` in ESP-IDF 5.5.5, use separate fresh build/config paths to avoid
an existing sdkconfig overriding defaults. These commands only build:

```sh
idf.py -B build-standalone -D SDKCONFIG=sdkconfig.standalone.generated -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.standalone" build
idf.py -B build-listen -D SDKCONFIG=sdkconfig.listen.generated -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.listen-only" build
idf.py -B build-default -D SDKCONFIG=sdkconfig.default.generated -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
```

Check generated CAN/host-injection options; do not reuse these config filenames
for a different preset. Factory backup, flash and physical acceptance are separate
steps in [REAL_CAR_TEST](../REAL_CAR_TEST.md). No acceptance runner flashes a board.

### Build from hash-verified local dependencies

The 2026-10-04 recovery uses this alternative when the component manager cannot
inspect the CMake process or attempts to change locked dependencies. It downloads
only versions already in `firmware/dependencies.lock`, verifies each package's
component hash, and never runs the version solver or rewrites the lockfile.
Use the ESP-IDF Python environment with `idf-component-manager==2.5.2`; these are
development dependencies, not part of the device runtime.

From the repository root in the initialized IDF environment:

```sh
python tools/prepare_locked_components.py --output artifacts/local/pinned-components
export IDF_COMPONENT_MANAGER=0
MINIDASH_COMPONENTS="$(pwd)/artifacts/local/pinned-components"
cd firmware
idf.py -B build-standalone -D SDKCONFIG=sdkconfig.standalone.generated -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.standalone" -D "EXTRA_COMPONENT_DIRS=$MINIDASH_COMPONENTS" build
idf.py -B build-listen -D SDKCONFIG=sdkconfig.listen.generated -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.listen-only" -D "EXTRA_COMPONENT_DIRS=$MINIDASH_COMPONENTS" build
idf.py -B build-default -D SDKCONFIG=sdkconfig.default.generated -D SDKCONFIG_DEFAULTS=sdkconfig.defaults -D "EXTRA_COMPONENT_DIRS=$MINIDASH_COMPONENTS" build
```

Use new build/config paths if an earlier configure used other dependencies.
PowerShell equivalents for the environment/path setup are
`$env:IDF_COMPONENT_MANAGER = '0'` and
`$miniDashComponents = (Resolve-Path artifacts/local/pinned-components).Path`;
pass `"-DEXTRA_COMPONENT_DIRS=$miniDashComponents"` to each build.
The helper refuses an existing output directory, preserves the lockfile, and
publishes the directory only after all packages pass hash validation. Its
`minidash-dependencies.json` records versions/hashes for review. Do not reuse
partially downloaded or mixed-version directories. Normal manager-based builds
remain supported outside this recovery workflow.

### Physical acceptance

Bench: English/Russian labels, all dashboard long-hold pickers, three/four Custom
pages NEXT/PREV wrap, Settings select without change, explicit CHANGE, Save/Cancel
and reboot/NVS/calibration retention. Check touch targets physically; backlight
must retain the owner-accepted GPIO4 PWM wiring. CAN TX15 is a proposed
separate installation pin, not physical acceptance. Record firmware hash and UI self-test.

For the preserved host research workflow, confirm protected power/transceiver/listen-only wiring and the intended
connection point; doctor/F190 and exact identity first, then only evidenced
read-only signals. Capture with the tagged manifest, compare against a reference,
replay offline and retain provenance. No clear/coding/unknown-button probing.

## Instrument catalogue consistency

After editing `data/ui/instrument-profiles.json`, run
`python scripts/generate_instrument_profiles.py`, then
`python scripts/generate_instrument_profiles.py --check`. The host uses the same
JSON while the MCU compiles the generated numeric table. Native CTest adds
`instrument-profile-contract` (matching ranges, strict frames, expiry and clock
wrap). Python `tests.test_instruments` covers observed-vs-seed identity and
repeated bridge profiles; device Web tests additionally verify range validation,
NVS roundtrip and local changes preserving manual ranges.

## Derived Analytics offline gates

Same `firmware/main` C files compile on ESP32 and natively. No CAN/serial/VCI is
opened by either replay verifier. Run the existing common acceptance command
with a cJSON-enabled native build; it now also requires all eight native groups, including car-validation
and runs `tools/verify_analytics.py` against `minidash-analytics-harness`.

```sh
cmake -S core -B core/build-analytics -D MD_JSON_SOURCE_DIR="$IDF_PATH/components/json/cJSON"
cmake --build core/build-analytics
python tools/verify_repository.py --output-dir .local/analytics-acceptance --native-build-dir core/build-analytics
python tools/verify_analytics.py --harness core/build-analytics/minidash-analytics-harness
```

`--synthetic` belongs only to the native harness: test thresholds and counter
widths never enter firmware defaults. Regenerate reviewed append-only catalogue
with `scripts/generate_analytics_catalog.py`, traces with
`scripts/generate_analytics_traces.py`; do not move old signal IDs. The engine's
RAM storage uses `EXT_RAM_BSS_ATTR` and requires the existing board PSRAM setup
plus `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y`. Engine tick has no heap
allocation; Web JSON allocation is bounded and outside the sample path.

Optional ASan/UBSan native build uses `-fsanitize=address,undefined`. On this
sandbox LeakSanitizer could not enumerate `/proc` threads; final address/undefined
checks ran with `ASAN_OPTIONS=detect_leaks=0`. This is not a claimed leak check.

The native `analytics-acquisition-pipeline` group uses the same raw/canonical
bridge as firmware, exercises complete supported-PID collection under normal
and overloaded latency, and requires loss/reconnect timing invalidation. See
[pipeline scheduling follow-up](research/analytics-pipeline-scheduling-2026-10-04.md).
