# MiniDash project instructions

## Finishing a task

The project owner requires completed task changes to be committed and pushed
to the remote repository at the end of every MiniDash task. This is standing
authorization: do not ask for commit/push confirmation again.

- Commit all final changes belonging to the task in logically scoped commits.
- Preserve unrelated local changes; do not include them merely to clean status.
- Push the current feature branch to `origin`, setting upstream when needed.
- Verify that the remote branch points to the final local commit.
- Report the branch, final commit and any push failure accurately.
- Do not force-push or merge into `main` without separate explicit authorization.

## Documentation shipped with every task

The project owner also requires detailed documentation of completed work and
remaining work to be committed and pushed with the implementation, every time.
A chat summary alone is not sufficient.

- Update the relevant existing documentation under `docs/`; add a dated task
  report when needed and link it from the relevant overview or README.
- Describe what changed, why, affected files/modules, architecture/API/config
  implications, compatibility, migrations and preservation of existing work.
- Record verification actually performed, commands/environment, results and
  limitations. Distinguish completed tests from planned or unavailable checks;
  do not claim vehicle or hardware validation from synthetic tests.
- Document remaining gaps and blockers, then prioritize concrete next steps.
  For each significant step, state the purpose, affected components, acceptance
  criteria, risks and required vehicle evidence, hardware or user input.
- Include applicable usage/recovery instructions and research provenance/license
  notes. Keep secrets and private/proprietary artifacts out of committed reports.
- Before pushing, ensure implementation and documentation agree. Historical
  reports may retain their original context; keep current status and next steps
  discoverable and accurate.
