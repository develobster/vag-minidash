# Analytics evidence collection: Tiguan II 2017 CZPA / DQ500 / Haldex

This is a read-only collection protocol, not permission to try unknown raw
requests. It supplements [standalone bench/car checks](../../REAL_CAR_TEST.md)
and the [VNCI transport prerequisites](first-real-car-session.md). No car was
connected during this implementation. Current native manufacturer profile is
NULL. The known Windows D-PDU provider is not a usable J2534 library; the host
recording commands remain BLOCKED until their existing transport gates pass.

## What is available through standard OBD

| Canonical source | Standard Mode 01 PID / definition | Acceptance needed |
|---|---|---|
| RPM / speed / calculated load | 0C / 0D / 04, existing codecs | Supported bitmap, reference match, timestamps and actual sample gaps |
| Coolant / oil temperature | 05 / 5C, A−40 °C | 5C is optional; do not substitute coolant for oil |
| MAP / BARO | 0B / 33, A kPa → canonical 10A hPa | Compare absolute pressures, then MAP−BARO; this is not VAG charge tracking |
| Module voltage | 42, (256A+B)/1000 V | Identify ECU reporting it; it is not battery-terminal measurement |
| IAT / ambient air | 0F / 46, A−40 °C | New optional sources, supported-PID and reference checks first |
| STFT / LTFT Bank 1 | 06 / 07, (A−128)×100/128 % | Optional; verify fuel-system/closed-loop context separately in reference capture |

These standard definitions are researched facts. Availability/accuracy on this
particular ECU remains unvalidated. The new PIDs are automatically scheduled only
when discovery explicitly reports support. Never send an unsupported PID merely
because the table includes it. OBD speed resolution is 1 km/h; the acquisition
request budget can make actual intervals slower than their nominal 200 ms.

There is no shipped standard OBD mapping for individual-cylinder retard/misfire,
VAG requested/actual charge pressure, selected gear, DSG or Haldex sources.

## Evidence packet for every manufacturer candidate

Create a new private directory per session. Preserve originals, timestamps/time
zone, software versions and SHA-256 hashes; commit only sanitized excerpts with
permission. Record VIN privately, ECU name, SW part/version, HW part/version,
ASAM dataset/version, VCI model/provider/version, diagnostic bus/channel/bitrate,
addressing format and **observed** physical request/response endpoints.

For each value save its exact ODIS label/identifier, unit, displayed value and
timestamp, raw request/service/DID, complete reassembled positive response,
payload length/offset/endianness/sign/scale/offset proposal and applicability.
Also save negative/unsupported responses and the timestamps of source updates.
The label alone, a screenshot or a decoded CSV is not sufficient wire evidence.
If raw logging cannot be exported by the installed tool, leave the codec/address
unknown and save only the conceptual measurement evidence.

Existing identities to compare, never silently substitute:

| ECU | Existing evidence identity | Narrow confirmed evidence |
|---|---|---|
| Engine J623 CZPA | SW 5NA907115D / 0003; HW 06L907309B / H31; EV_ECM20TFS0115NA907115D / 001004 | 7E0/7E8, 500 kbit/s, normal 11-bit; F405 payload A−40 coolant samples. Native activation still needs reviewed live fingerprint/layout gate |
| DQ500 | SW 0DL300011P / 2004; HW 0GC927711G / H06; EV_TCMDQ500021 / 001001 | Displayed 2104 ATF and 3816 gear observations; raw codec/physical endpoint not confirmed |
| Haldex | SW 0CQ907554H / 7083; HW 0CQ525130 / H52; EV_AllWheelContrHA1VW37X / 004013 | 2BF1 clutch temperature, 028D module, 2BE4 fin and pump/status observations; raw codec/physical endpoint not confirmed |

Existing DIDs in this table are evidence references, **not** a raw-request list.
Select only a measuring value exposed by the installed ODIS dataset on the
identified ECU. Do not paste these identifiers into an experimental request tool.

## Exact read-only ODIS/VNCI sequence

1. Before connection record repository commit/build/config and run the existing
   offline preflight/driver inventory from `first-real-car-session.md`. Do not
   replace/upgrade the working ODIS driver. Confirm how the installed ODIS exports
   ordinary measuring-value logs and diagnostic/VCI traces using its documented
   logging facility. Menu labels vary between ODIS Service/Engineering versions;
   do not enable an undocumented logger, DLL hook or memory extraction.
2. On a stationary vehicle: P, parking brake, normal ventilation, stable supply.
   MiniDash and other active diagnostic clients disconnected. Connect VNCI, use
   the existing licensed ODIS vehicle identification workflow. Open only the
   identified control unit's **identification / measured values** view. Do not
   run guided test plans, actuator tests, coding/adaptation, basic settings,
   security access, RoutineControl, writes or DTC clearing.
3. Save the identification screen/export for Engine, Transmission and AWD,
   one control unit at a time. Start a fresh measuring-value/raw trace file for
   each group below. Capture tool timestamps and add an external marker log via
   `tools/real_car_session.py marker LABEL --directory PRIVATE_SESSION_DIR`;
   this marker command only writes a local file and opens no vehicle connection.
4. With ignition ON/engine OFF, capture 30 s of the selected supported group.
   Mark ENGINE_START, then capture 60 s stable idle. Retain the response lengths
   and actual per-value intervals. Limit groups to roughly 4–8 values so the
   tool's sample rate remains interpretable. Do not assume four sequentially
   read cylinders are simultaneous.
5. After normal warm-up, repeat 60 s stable idle. A short light-load reference
   passage may be logged during an ordinary safe journey with an assistant
   operating the recorder; do not induce knock/misfires, lug the engine, force
   boost faults, unplug sensors or perform 100–200 runs on public roads. Loaded
   validation requiring repeatable conditions belongs on an appropriate test
   facility. Standstill RPM alone does not prove load-related diagnostics.
6. End logging, disconnect diagnostic ownership cleanly. Save raw/decoded exports
   and markers before another client connects. Audit the trace service list. If
   the tool emits any prohibited service or requires one to read a candidate,
   stop that group and mark it BLOCKED. No MiniDash feature will rely on a
   session/security/routine/write sequence under this task's safety rules.
7. Only after the standalone wiring/listen-only gate passes, run a separate
   MiniDash standard-OBD session. Compare stable conditions against the ODIS
   reference; never call concurrent ODIS diagnostic traffic passive baseline.
   Check unsupported/stale UI states, peak resets, voltage source labels and
   timing invalidation on a bench replay/disconnection. Do not disconnect the
   vehicle bus during motion.

## Measuring groups and promotion criteria

| Group to select by exact supported ODIS label | Required context / collection | What blocks activation |
|---|---|---|
| Ignition correction/retard cylinders 1–4 | RPM, calculated load, coolant/IAT; zero and naturally occurring nonzero values; verify negative retard versus positive advance convention, degree unit and cylinder order | No nonzero raw/display pairs or no sign/unit/layout proof. Retard threshold episodes cannot establish physical knock-cycle count |
| Misfire counters cylinders 1–4 | RPM/speed/load, recognition enabled/disabled value if exposed; repeated samples, natural increments, ignition cycle/restart baseline | Width, reset epoch and wrapping semantics unknown. Never deliberately induce a misfire. Unknown wrapping stays disabled; real increments needed before rate acceptance |
| STFT/LTFT Bank 1 | Standard PID captures plus ODIS exact labels; fuel-system state/closed-loop, idle and ordinary light load | ODIS additive idle trim with a different unit is not automatically STFT %. Do not equate sum with effective correction or engine health |
| Requested and actual charge pressure | Paired source timestamps, BARO/MAP/RPM/load; idle and normal changing load; record absolute/gauge meaning and sensor location | MAP and manufacturer charge pressure are not interchangeable. No requested DID/endpoint known; error remains disabled |
| IAT | Standard 0F capability/raw pairs, ODIS label/location; cool/warm idle, ambient and ordinary movement | Ambient PID46 may be unsupported or update slowly; stale ambient invalidates delta. Heat soak remains a configured proxy |
| DSG ATF temperature / selected and current gear | Separate Transmission identification and endpoint trace; normal warm-up; supported gear labels at parked P/R/N and an ordinary journey | 2104 raw scale unconfirmed; 3816 observed NO_GEAR/R/1 is not a universal gear enum. Do not infer P/N or map selected=current |
| Haldex clutch/module/fin temperatures and pump activity | Separate AWD identification/endpoint; each exact unit/name, ordinary stationary/driving states, pump current/voltage/PWM if exposed | No endpoint/raw scalar layouts. Pump current is not clutch torque; PWM is not engagement %, structured status is not proven hydraulic pressure |

Promotion is a reviewed change: preserve candidate status → validate independently
on multiple raw/display points → add exact-identity/address/codec evidence plus
negative cases → replay the same decoder in host and native C → explicitly enable
only those bindings → bench test → repeat on this car and record acceptance.
Internet DIDs, generic MQB DBC matches or a matching model name do not bypass this.

## Next acceptance priorities

1. Verify GPIO15 TX / GPIO5 RX wiring independently of the accepted GPIO4 TFT
   backlight. Bench cold boot, PSRAM BSS, touch, Wi-Fi editor, existing themes,
   schema-3 migration/NVS save/reboot and Focus/picker readability (all 92 IDs).
2. Listen-only TX=0, then standard OBD capability collection and timing/TTL checks.
   Compare the new IAT/ambient/trims, ECU voltage and observed thermal/boost peaks.
3. Resolve licensed VNCI transport/logging prerequisites; capture exact identities
   and per-cylinder/charge/DSG/Haldex wire evidence using the sequence above.
4. Set user alert/context thresholds only after choosing them for this vehicle;
   no factory safety limits are supplied. Validate events/reset behaviour by
   offline replay before enabling alerts on the device.
5. For performance, first prove actual speed sample gaps ≤1 s and loss invalidation;
   reference timing comparison on a safe facility precedes any accuracy claim.
