# Analytics integration base audit — 2026-10-04

All remote branch heads were inspected. `feature/production-ui`,
`feature/ui-ux-touch-zones`, `feature/mqb-knowledge-import`,
`feature/research-replay-reliability`, `feature/diagnostic-data-import`,
`feature/esp32-display-prototype`, `runtime/session-scheduler`, consolidation and
main are ancestors of standalone `ab1c09c`. `feature/real-car-validation`
`bf7e8ba` diverges at `3e5c6ca` (seven commits versus four standalone commits).
The new branch integrates both parents before analytics work. Main is unchanged.

Preserved: standalone transport, strict profile gate, DTC read/quiet/recovery,
VirtualBus harness and dependency-lock builds; real-car read-only capture/replay,
backlight PWM, display rotation, eleven themes, CRT startup, Focus needles,
vehicle instrument profiles and schema-3 gauge ranges.

Conflict resolutions combine source lists/tests, vehicle diagnostics and Wi-Fi
status, standalone demand with rotation and the newer backlight implementation.
USB instrument/profile and telemetry injection both remain development-only.
No manufacturer profile is enabled. The known GPIO4 backlight conflicts with the
old CAN TX4 installation proposal: new CAN installation presets specify TX15/RX5.
This is a proposed transceiver wiring, not new physical acceptance; verify GPIO15
on the board before wiring. Existing pin collision checks still fail closed.

Historical standalone reports describe the previous UI baseline. Their sizes and
pin plan must not be interpreted as the integrated release measurements.
Fresh integrated test/build results will be recorded in the analytics report.
