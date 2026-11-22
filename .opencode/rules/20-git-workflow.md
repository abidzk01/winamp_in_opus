# Rule: Git workflow

## Before committing

1. Run `git status` and `git diff` (plus `git diff --staged`) and know exactly
   what is being committed.
2. Stage explicit paths (`git add in_opus.c infobox.h`), not `git add -A`, unless
   every change is yours and intended.
3. Verify the change still builds, or state clearly that it was not built.
4. Commit only when the user asks for a commit, and push only when the user asks
   for a push.

## Commits

- Follow `.opencode/rules/10-conventional-commits.md`; message in English.
- One logical change per commit; keep refactors and formatting separate.
- Never commit build output or local library trees: `*.o`, `*.res`, `*.a`,
  `*.dll`, `oflto*/`, `logg/`, `opus/`. If one of them shows up as untracked,
  fix `.gitignore` — do not use `git add -f`.
- `AGENTS.md`, `opencode.json` and `.opencode/` are part of this repository: keep
  them in sync with the code and commit them together with the change they
  describe.
- Never commit secrets, credentials, or personal paths from a local `Makefile`.

## History

- `main` is published on GitHub (`origin` → `RamonUnch/in_opus`). Never rewrite
  it: no `git commit --amend` on pushed commits, no `git rebase` of shared
  history, no `git push --force`/`--force-with-lease`.
- To undo published work use `git revert` or a new `fix:` commit.
- Do not delete branches, tags, remotes, or stashes without explicit user
  confirmation.
- Branches created by other people (`RamonUnch-*`, pull-request branches) belong
  to them — do not rebase, force-push, or delete them.

## Branches, PRs and releases

- Branch naming: `<type>/<kebab-case-summary>`, using a type from the commit
  types — e.g. `fix/utf8-overlong-sequences`, `feat/format-title-option`,
  `docs/readme-refresh`.
- One topic per branch, branched off the current `main`.
- Pull requests: English title in conventional-commit form, description covering
  what changed, why, how it was tested (which players/OS), and references such as
  `Closes #14`.
- Land changes into `main` through a merge commit, matching the existing history.
- Releases: annotated tag `vX.YZ` on the version-bump commit of `main`, following
  `.opencode/skills/in-opus-build/references/release-checklist.md`.

## Safety

- Do not run destructive commands (`git reset --hard`, `git clean -fdx`,
  `git checkout --`, `git push --force`) without explicit confirmation, and never
  on files you did not author in this session.
- Do not change `git config` (global or repository) without being asked.
- Report the exact commands you ran for anything that changed repository state.
