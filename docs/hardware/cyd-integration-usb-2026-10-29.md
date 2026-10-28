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
