# Current architecture

Integration base: 22a346b, audited on 2026-09-30. Architecture and runtime source
paths are retained. Consolidation changes documentation, ignore rules and checks.

```text
Transport -> ECU discovery -> VehicleIdentity/EcuIdentity -> IdentityContext
  -> Knowledge Providers -> Signal Registry -> Acquisition Planner
  -> passive CAN / UDS polling -> SignalStore -> SourceArbitrator
  -> UI / CLI / debug / optional USB canonical bridge
```

| Boundary | Existing implementation | Contract |
|---|---|---|
| Wire/VCI | `minidash_live/transports.py`, `j2534.py`, `uds.py` | Evidenced addresses, bounded transactions, response validation; no default broadcast |
| Discovery/identity | `discovery.py`, `identity.py` | Configured endpoints and observed identity; snapshot alone does not validate a decoder |
| Knowledge | `knowledge.py`, `vag.py`, `registry.py`, `mcd_import.py`, `dbc.py`; `data/` | Provider/applicability/provenance gates; canonical/raw namespaces stay separate |
| Planning/acquisition | `pipeline.py`, `runtime.py`, `passive.py` | Passive grace, fallback eligibility, bounded scheduler, reconnect/backoff |
| Values | `runtime.py:SignalStore`, `arbitration.py` | Timestamp/TTL/quality/source selection; unavailable values are not zero |
| CLI/trace | `__main__.py`, `api.py`, `display_bridge.py`, `can_capture.py` | Canonical consumers and receive-only recorder; trace creation preserves existing output |
| Offline replay | `replay.py`, `can_capture.py:CanTraceReader` | Existing engine feed/snapshot path; injected clock; no transport or ECU session |
| Embedded model | `firmware/main/ui_core`, `ui_model`, `ui_configuration`, `ui_platform` | Canonical sample vocabulary, navigation/config/presentation; no vehicle IDs |
| ESP32 adapters | `display_app`, `ui_screens`, `ui_web`, `*_nvs`, panel/touch backends | LVGL, NVS, temporary AP/HTTP, SPI hardware |
| Native core | `core/CMakeLists.txt` | Compiles eight of the same physical C modules, without LVGL/ESP-IDF |

The embedded presentation store is not the host acquisition SignalStore or
SourceArbitrator. The current firmware vocabulary contains twelve canonical
signals; registering host candidates does not automatically expose them on the
device. `core.py`, `diagnostics.py` and `transport_lab.py` are retained older
simulator/lab paths, not live DTC/coding implementations.

Safety boundaries: default live UDS uses no explicit session change; extended
session/keepalive require opt-in. Exact identity is rechecked on reconnect.
Unsupported/candidate signals are not promoted by replay. Web configuration
changes device settings only and has no diagnostic write endpoints. Driving
gates lack a verified live vehicle-speed source today.

Details: [knowledge](vehicle-knowledge.md), [end-to-end runtime](../runtime/end-to-end.md),
[embedded UI](../ui/architecture.md), [configuration](../ui/configuration.md),
[targets and shared core](targets.md), [replay](../runtime/can-replay.md).
