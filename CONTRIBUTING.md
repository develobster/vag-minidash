# Contributing to MiniDash

Current development baseline: `integration/standalone`. Read [AGENTS.md](AGENTS.md)
and [development instructions](docs/development.md) before changing the project.
Keep routine continuation on the existing baseline and preserve historical branches.

Include a clear problem statement, affected components, and verification actually
performed. Separate synthetic/offline results, bench checks and real-vehicle
evidence. Do not promote a CAN/UDS definition to a validated binding without the
required ECU identity and trace evidence.

Use the existing Python, native C, Web and firmware checks appropriate to the
change; commands and dependencies are in [docs/development.md](docs/development.md).
Documentation-only changes should verify links and agreement with current code.
Do not flash hardware or perform vehicle operations as part of an ordinary test run.

Never include real VINs, credentials, private captures, OEM databases, factory
backups or build output in a contribution. Use reviewed synthetic fixtures and
retain source/license notices. See [artifact policy](docs/artifacts.md) and
[fixture notices](tests/fixtures/README.md).

Project-authored code is licensed under [MIT](LICENSE). Third-party materials
retain their existing license notices. See the
[publication preparation report](docs/development/open-source-readiness-2026-10-17.md).
No contributor license agreement or rights assignment is introduced here.
