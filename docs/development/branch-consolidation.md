# MiniDash branch consolidation — 2026-10-05

The sole development baseline is **`integration/standalone`**. Old refs are
preserved; no force push, ref deletion, `main` merge or default-branch change is
part of this delivery. The remote default remains `main`.

## Evidence and baseline choice

Fetched every advertised origin head and examined every local branch, all three
existing worktrees, the commit graph, merge parents, unique commit/file diffs,
the local stash and uncommitted changes. Machine-readable SHAs/counts are in
[branch-audit-2026-10-05.json](branch-audit-2026-10-05.json).

`751d2f8417a6e8dd54a6c6fffff4e522f80db49f` has **zero missing commits from every
one of the 12 remote branches**. This is an ancestry result, not a date/name
assumption. It includes merge `874c202` (UI + MQB knowledge), consolidation
`3e5c6ca`, standalone `ab1c09c` and merge `6ae2ed4` (standalone + real-car UI
`bf7e8ba`), followed by bounded analytics and scheduler fix `2a124f7`.

Its tree contains the current display/touch/navigation/settings/themes/layouts,
standard CAN/ISO-TP/OBD and evidence-gated UDS/passive core, canonical bridge,
derived windows/history/timers/events/validity, fail-closed MQB/diagnostic
knowledge, native/replay/acceptance harnesses and bounded stationary DTC reads.
It does not contain the newer **local-only mobile host Web Configurator**.

## Branch → unique commits → functionality → integrated → useful → archive

Unique counts below are relative to `751d2f8` before this work. Parent counts and
full SHAs are in the JSON. "Safe-to-archive" concerns committed history after
origin integration verification, not permission to delete a ref or dirty checkout.

| Branch / audited tip | Unique commits | Functionality and integration evidence | Status / useful? / archive |
|---|---:|---|---|
| `main` / `de7f071` | 0 | Audited v0.9 ancestor, 54 baseline-only commits | Historical; preserve default branch; no automatic archive |
| `runtime/session-scheduler` / `e62ca59` | 0 | Early UDS sessions/passive store; ancestry retained, newer bounded MCU scheduler is production | Superseded; history useful; safe-to-archive |
| `feature/diagnostic-data-import` / `54d0ea5` | 0 | Registry, identity/provider boundaries, scalar import, first Engine route; retained | Superseded; useful code/evidence retained; safe-to-archive |
| `feature/esp32-display-prototype` remote / `dc16a90` | 0 | ST7789/ILI9341 and XPT2046 foundation; retained through production UI | Superseded; safe-to-archive remote history only |
| `feature/production-ui` / `00c6317` | 0 | LVGL canonical UI/settings/USB bridge; retained by `6f436dd` and later merges | Superseded; useful; safe-to-archive |
| `feature/ui-ux-touch-zones` / `6f436dd` | 0 | Configurable portable UI/touch/local AP editor and targets; retained | Superseded; useful; safe-to-archive |
| `feature/mqb-knowledge-import` / `0b3121d` | 0 | Provenance/scoped MQB candidates/evidence gates; merged by `874c202` | Superseded; useful fail-closed research; safe-to-archive |
| `feature/research-replay-reliability` / `22a346b` | 0 | Offline CAN replay, watch timing, typed MCD recovery, branch research; retained | Superseded; useful; safe-to-archive |
| `feature/repository-consolidation` / `3e5c6ca` | 0 | Current/historical docs, artifact protection and acceptance runner; retained | Superseded; useful; safe-to-archive |
| `feature/real-car-validation` / `bf7e8ba` | 0 | Strict host evidence/replay plus PWM, rotation, themes and Focus scales; `6ae2ed4` | Superseded; useful; preserve dirty worktree before archive |
| `feature/standalone-acquisition` / `ab1c09c` | 0 | MCU CAN/ISO-TP/OBD/UDS runtime, DTC read, quiet/recovery, locked builds; `6ae2ed4` | Superseded; useful; safe-to-archive |
| `feature/engine-analytics` / `751d2f8` | 0 | 92 canonical IDs, 66 derived nodes, statistics/history/events/timers and full-PID freshness fix | Selected baseline, now superseded by integration; safe-to-archive after delivery |
| **Local** `feature/esp32-display-prototype` / `bdd5f7a` | **7** | Mobile host configurator, canonical SSE/logging, settings v2/capabilities/secrets and browser tests | **Contained unique work; normal merge into integration; useful; dirty checkout preserved** |

All other local branch tips match the remote tips in this table. No extra
development branches were created. `integration/standalone` uses a separate
checkout because the original display checkout has unrelated uncommitted work.

### Recovered seven commits

`3dfd240`, `f99e9cc`, `6a63edc`, `f72b6da`, `93a88a0`, `6686c6d`, `bdd5f7a`:
host configuration/SSE/recording API; mobile editor/themes/offline shell; source
selection proof; capability-validated settings and protected Wi-Fi secrets;
mobile settings sheets/search/units/Apply; scroll reset; contracts/visual proof.
The normal consolidation merge retains their original ancestry. README and
`minidash_live/__main__.py` conflicts combine both paths: serial instrument
profiles, watch duration after setup, offline replay CLI, and Web publication.
Neither host configurator nor serial bridge becomes a production dependency.

### Local changes/stash disposition

The original display worktree's typed MCD FLOAT32/TEXTTABLE recovery and fixture
were already integrated at `013d350`. Its older endian validation, profile signed
defaults, provenance fields, applicability tests and source notes are superseded
by the stricter baseline; copying the dirty diff would regress them. Hardware and
ODIS extraction artifacts remain private and untouched.

The real-car worktree has five uncommitted documentation edits. Two stale FIXED
backlight claims in targets/UX were corrected using the existing committed GPIO4
acceptance record. The rest mostly changes an old VNCI-first roadmap; current
standalone priorities replace that roadmap in the new report. These unrelated
working changes remain in their original checkout.

The local stash `a09cf6f` contains consolidation docs/tests/runner already committed
by `34aa05f`/`b1d38c1` and subsequently improved. Exact artifact/history/acceptance
files match; current overview/runner are newer. No stash pop or duplicate transfer.
The production UI worktree is clean. No user changes or original worktrees are reset.

## Future work

Use `integration/standalone` for subsequent fixes. Do not treat an old README or
feature branch as the latest product. No obsolete PC-only runtime is promoted;
historical simulator/research code is retained with an explicit scope. Archive
decisions are documentation only: **all old remote branches still exist**.
