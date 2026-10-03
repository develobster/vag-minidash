# Standalone completion — 2026-10-03

Historical hardening checkpoint. For current delivery/evidence and recovery
limitations, see [the final report](standalone-final-2026-10-03.md).

Continuation of `feature/standalone-acquisition`, exact recovered checkpoint
`18095539db54161509ceeb66967579313f0ce4aa` (191 verified blobs; tree
`67cd86649f9ba5a23df8644b70bdd3d85c2c0e32`). Existing source and history preserved.

## Startup and protocol hardening checkpoint

- `vehicle_app_prepare()` creates the status mutex and initial state before UI
  tasks can read them. `vehicle_app_start()` starts the CAN producer only after
  display store initialization. Task-creation failure and engine initialization
  use the same mutex as status/trace readers.
- ISO-TP single/first-frame validation checks the declared complete response
  length as well as address/service/PID/DID before Flow Control. A matching RPM
  prefix with an impossible multi-frame length cannot trigger CAN TX.
- Regression is included in `core/test_acquisition.c`.

Verification at this checkpoint: portable CTest 2/2 PASS (acquisition, UI core),
including 50,000 malformed-frame inputs. Python 164 tests initially had one
environment error because the sandbox denied a local test socket; unrestricted
rerun is in progress. ESP-IDF 5.5.5 tooling is being restored. Firmware builds,
JSON/editor and VirtualBus gates are not yet claimed complete for these changes.
No physical-board or vehicle test has occurred.

Remaining completion work: full offline acceptance, three firmware builds,
source/license/competitor report and `REAL_CAR_TEST.md`. Quiet is still not
electrical power-off; use the physical switch for the current prototype.
