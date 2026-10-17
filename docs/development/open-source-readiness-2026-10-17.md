# Open-source publication readiness and Codex application — 2026-10-17

## Status

Owner approved MIT adoption and public exposure including existing commit
history on 2026-10-17. Root LICENSE is now MIT; third-party notices are retained.
Repository visibility is now public and the default branch is
integration/standalone. GitHub's settings page and repository metadata both
confirmed the change on 2026-10-17 after authenticated administrative access. The Codex application remains unsubmitted because the
applicant's full name, exact ChatGPT email and Organization ID are missing.

Current development baseline: `integration/standalone`, reviewed at
`1de3133ec713ba98117e04dabc1ea0de1238d626`. Its existing consolidation includes
the display/stack work at `da1e6b7`; the older default `main` is
`de7f071a2d7e574970614527d4397a274fa46813`.

## Review performed

Read GitHub repository metadata, all 14 branch heads, commit histories and the
recursive trees of all 72 unique commits reachable from those branches. None of
the branch histories reached the 100-item retrieval limit. The returned Git refs
contained no tags; the additional PR #1 merge tree contained no new blobs.
The union contained 815 unique file blobs (different historical versions count
separately), of which 813 were readable text.

Ran text-pattern checks across those 813 versions for GitHub/OpenAI/AWS/Slack
token formats, private-key headers, credential-bearing URLs, bearer credentials,
secret assignments, VIN-shaped strings, email addresses, local user paths,
IPv4 addresses and the known internal corporate host suffixes. Reviewed matched
examples and current artifact/source-license documentation.

Results:
- No obvious live credentials, private keys, real VINs, email addresses or
  known internal corporate hostnames found by these checks.
- VIN-shaped strings were synthetic `WVWZZZ00000000000` and
  `TESTVAN1234567890`.
- Credential matches were test values, templates or runtime formatting.
- Address/path matches included loopback, example/private LAN addresses and
  generic Windows `Users/user` paths in historical instructions.
- No tracked factory flash image or raw OEM database was identified in the
  reviewed file trees. Existing private-artifact exclusions are documented in
  [artifacts](../artifacts.md).
- Both PNG icons were retrieved through base64 file access, inspected visually
  and parsed: plain MiniDash M icons, only IHDR/IDAT/IEND chunks, no auxiliary
  metadata.
- The Actions runs endpoint returned zero runs. The issue listing returned only
  PR #1; its body was reviewed and its discussion endpoint returned no comments.
- All 72 commits used one personal Gmail identity in author/committer metadata.
  The address is intentionally omitted from this report; making history public
  exposes it. Obtain the owner's decision about that disclosure before publication.

Limits: these are heuristic text checks, not an exhaustive secret detector or a
security audit. Commit author metadata was reviewed but not redacted; author name/email and
dates become public with Git history. Unreachable objects, deleted refs, local
worktrees/stashes, ignored files, LFS payloads and private external artifacts were
not reviewed. Repository settings and any surfaces not returned by these endpoints remain
outside the review. No attachments were identified in the returned PR body. No license clearance of every source line is claimed.

## License proposal

[Root MIT license](../../LICENSE) was adopted with the owner's approval on
2026-10-17. The [historical proposal](../governance/MIT-LICENSE-PROPOSAL.txt) is
retained for context. MIT allows reuse, modification, distribution and commercial
use with the notice retained. Original third-party terms remain
applicable: opendbc fixture notices are in
[tests/fixtures/README.md](../../tests/fixtures/README.md); bundled font OFL
notices remain beside the generated font files. Dependency and upstream rights
are not replaced by the proposed root license.

Current README, contributor guidance and source matrix now point to root MIT.
Dated historical observations retain their context.

## Prepared publication changes

- Added a concise English project introduction and a pointer to this review.
- Prepared contributor guidance covering baseline, reproducible checks and
  exclusion of private vehicle/OEM artifacts.
- Prepared the proposed MIT license without changing licensing status.
- Prepared the three English application responses below, each under the
  official form's 500-character limit.
- No runtime code, protocol settings, firmware, dependencies or schemas changed.
  No merge into main, branch deletion, history rewrite, flashing or vehicle I/O.

Verification: checked response character limits and repository-relative links
in the added/modified documentation. Runtime/build tests were not rerun because
these changes only affect documentation. Prior bench/vehicle reports remain
historical evidence; this task provides no new hardware or vehicle validation.

## Codex for Open Source application draft

Official form: https://openai.com/form/codex-for-oss/
Program terms: https://learn.chatgpt.com/docs/codex-for-oss-terms

GitHub: `develobster`
Repository: https://github.com/develobster/vag-minidash
Role: Primary maintainer.

### Why does this repository qualify? (478/500)

MiniDash is an early-stage automotive telemetry project for affordable ESP32-S3 hardware. It combines CAN/ISO-TP/OBD acquisition, a touchscreen dashboard, a web configurator, and Python research/replay tools. As its primary maintainer, I maintain firmware, protocol validation, tests, and documentation. Its intended value is reproducible, evidence-based vehicle diagnostics without dependence on a phone or proprietary dashboard software; broad adoption is not yet established.

### How will you use API credits? (489/500)

I would use API credits for maintainer workflows: reviewing firmware and protocol changes, generating regression tests from sanitized or synthetic CAN/UDS traces, triaging issues, and checking documentation against implementation. The focus would be parser robustness, bounded memory and polling behavior, and reproducible offline validation. These are proposed uses; API automation is not yet deployed. Private vehicle traces and proprietary OEM databases would stay outside the workflow.

### Anything else? (433/500)

I am the project's author and primary maintainer. Development is consolidated in integration/standalone, with firmware, web tools, tests, and dated engineering reports. Display/touch bench acceptance is documented; full vehicle acceptance remains pending. I already use Codex for implementation, review, and regression work. The project is young, and I do not claim established adoption or compatibility beyond the recorded evidence.

The API workflows above are proposed uses, not deployed integrations.
Do not invent users, downloads, stars, releases or compatibility claims.
Personal account data must be supplied privately to the form, not committed here.

## Remaining actions and acceptance

1. DONE: owner approved MIT; root LICENSE and current documentation updated.
   Existing third-party notices were preserved.
2. DONE: owner confirmed public disclosure of history, including its Gmail
   identity. Recheck refs and settings before publication. PNG and returned
   Actions/issue/PR surfaces were reviewed.
3. DONE: changed the default branch to integration/standalone through GitHub
   settings, with no code/history merge into main.
4. DONE: made the repository public after the owner's confirmation. GitHub's
   settings and repository metadata confirm public visibility; existing branches
   and history can now be copied by others.
5. Complete legal first/last name, exact ChatGPT-account email and OpenAI
   Organization ID in the application. A new ChatGPT account is unnecessary.
   Never commit account identifiers, credentials or the completed personal form.
6. Submit only after confirming the program terms at the final action.
   Acceptance is discretionary; submission does not activate Pro immediately.

After each completed administrative step, update this report with verified
results. Publication is complete; application submission is still pending. Documentation-only
adoption changed no runtime code; no firmware/build tests were rerun.
