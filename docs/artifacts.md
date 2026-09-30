# Artifacts, privacy and retention

| Type | Storage policy | Reason |
|---|---|---|
| Source, dependency lock/default config, schemas, reviewed public metadata | Git | Reproducible implementation |
| Synthetic JSON/DBC/capture fixtures with origin/license notice | `tests/fixtures/`, Git | Repeatable offline tests; never vehicle evidence |
| Reviewed sanitized validation summaries/manifests | `data/`, `docs/runtime/`, Git | Exact applicability/provenance, no raw private dump |
| ESP-IDF/CMake outputs, SDK config, managed components | Local ignored build folders | Regenerated using tracked defaults/lock |
| Python/Node/IDE caches and local environments | Local ignored | Machine state |
| Factory flash backups, `.bin` dumps and hardware logs | Local `artifacts/hardware/`, ignored | Recovery/privacy; irreplaceable until separately backed up |
| Proprietary MCD/ODIS extraction and audit output | Local `artifacts/odis-offline-audit/`, ignored | OEM licensing and potentially identifying data |
| CAN/UDS traces, captures, connection config, VCI reports | Local ignored capture/trace folders | VIN/ECU identifiers, routes and local paths |
| Temporary acceptance logs/fixtures/JSON summaries | `.local/` or external task output | Diagnostics, not new provenance |
| Reviewed research reports / meaningful design SVGs | `docs/research/`, `docs/ui/`, Git | Engineering history |
| Raw screenshots / generated reports | Local by default; review individually | Secrets/license/size/content review before versioning |

The factory backup `esp32s3-factory-16mb.bin` remains in the original worktree.
SHA-256: `CD0F30AC9491DA499EAF2167F07B5ACCF23CF6779C506110431AA766F346273E`.
Consolidation neither erases nor overwrites it. A hash is integrity evidence, not
a second recoverable copy. Confirm an independent copy and restore procedure
before considering removal. No binary backup or OEM output is uploaded by this task.

`.gitignore` is a staging guard, not deletion or redaction, and does not untrack
files already in Git. Reviewed test fixtures are explicitly allowed for capture
extensions. Do not use `git add -f` on ignored captures without a provenance and
privacy review. Exported public reports must not include VINs, AP passwords,
session tokens, proprietary databases or entire private trace paths.

For local archaeology, retain patches, hashes and original files. Do not clean
or archive an occupied worktree with dirty or ignored artifacts until its owner
has verified recovery copies. There is no bulk cleanup/delete command here.
