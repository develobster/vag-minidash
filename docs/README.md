# Documentation index

Use [status](status.md), [development](development.md), and
[architecture overview](architecture/overview.md) as the current entry points.
Dates/test counts in historical delivery reports are not current acceptance.

## Current references

| Topic | Documents |
|---|---|
| Integration baseline / branches / acceptance | [Consolidation audit](research/repository-consolidation-2026-09-30.md) |
| Feature evidence | [Status matrix](status.md), [UI implementation/hardware record](ui/status.md) |
| Host diagnostic runtime | [End-to-end](runtime/end-to-end.md), [knowledge](architecture/vehicle-knowledge.md) |
| Device architecture / portability | [UI architecture](ui/architecture.md), [targets](architecture/targets.md) |
| Configuration / settings | [Configuration](ui/configuration.md), [settings](ui/user-settings.md), [USB bridge](ui/host-bridge.md) |
| Hardware and display/touch | [ESP32 bring-up](hardware/esp32s3-bringup.md), [display prototype](hardware/display-prototype.md) |
| First physical vehicle gate | [Current first-session runbook](validation/first-real-car-session.md), [preparation/readiness report](validation/real-car-preparation-2026-09-30.md); earlier [Tiguan first live](runtime/tiguan-first-live.md), [MQB validation](runtime/tiguan-mqb-live-validation.md), [manifest](runtime/tiguan-mqb-validation-manifest.json) |
| Offline evidence processing | [CAN replay](runtime/can-replay.md), [artifacts](artifacts.md), [fixture notices](../tests/fixtures/README.md) |

## Research: preserve, check against code before implementation

- [Diagnostic data sources / importer scope](research/vag-diagnostic-data-sources.md)
- [Ecosystem research and archaeology](research/ecosystem-2026-09-30.md)
- [Research/replay/MCD delivery results](research/ecosystem-2026-09-30-results.md)
- [UX design alternatives](ui/ux-design.md) and adjacent SVGs: design references,
  not screenshots proving physical acceptance.
- [Transport/VNCI investigation](vnci.md), [earlier DID research](research.md).

## Historical or version-specific notes

Preserved paths keep old links working. These documents may contain earlier
architecture, test totals, session defaults or planned functionality:

- [Previous README](history/README-pre-consolidation.md): version 0.6/0.8/0.10 narrative.
- [Technical audit](TECHNICAL_AUDIT.md), [early audit](audit.md),
  [runtime fork](RUNTIME_FORK.md), [PC backend](pc-backend.md).
- Root [ARCHITECTURE](../ARCHITECTURE.md), [HARDWARE](../HARDWARE.md),
  [DIAGNOSTICS](../DIAGNOSTICS.md), [TRANSPORT](../TRANSPORT.md),
  [RESEARCH](../RESEARCH.md): original design/lab notes. Simulator DTC/coding and
  intended CAN hardware must not be interpreted as current device features.

No research/evidence was removed. One duplicated navigation purpose is resolved
by this index; historical text is retained, rather than merging contradictory
versions into an apparent current specification.
