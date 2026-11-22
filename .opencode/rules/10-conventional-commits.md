# Rule: Conventional Commits (English)

Every commit message in this repository MUST follow
[Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/) and
MUST be written in English.

## Format

```text
<type>[(scope)][!]: <subject>

[body]

[footer(s)]
```

- `subject`: imperative mood, present tense, no trailing period, lowercase start
  ("fix crash on empty tag", not "Fixed crash on empty tag."), ≤ 72 characters.
- `body` (optional): what and **why**, wrapped at 72 columns, separated from the
  subject by one blank line. Reference findings, RFCs, or the failure mode.
- `footer` (optional): `Refs: #14`, `Closes #15`, `Co-authored-by: …`.
- Breaking change: add `!` before `:` **and** a footer starting with
  `BREAKING CHANGE:` describing the impact on users or the binary interface.
- One logical change per commit. Do not mix a refactor with a feature or a
  version bump.

## Types

| Type | Use for |
| ---- | ------- |
| `feat` | new user-visible capability (new option, new stream support) |
| `fix` | bug fix (crash, wrong output, wrong tag, broken seek) |
| `perf` | performance or binary-size improvement without behavior change |
| `refactor` | internal restructuring, no behavior change |
| `style` | formatting/whitespace only, no code change |
| `docs` | `README.md`, `in_opus.txt`, `AGENTS.md`, rules, skills, comments |
| `build` | `Makefile*`, `resource.rc` version plumbing, toolchain flags, linking |
| `ci` | automation (currently none configured) |
| `chore` | maintenance that fits nothing above (release bumps, cleanups) |
| `test` | test or verification tooling (currently manual) |
| `revert` | reverting a previous commit |

Anything not in this list needs a reason in the commit body.

## Scopes for this repository

Use the module or area you touched; omit the scope only when the change is
genuinely global.

| Scope | Covers |
| ----- | ------ |
| `plugin` | `in_opus.c` — decode thread, seek, replaygain, `In_Module`, config parsing |
| `tags` | `infobox.c`, `resource.rc` dialogs, Alt+3 display |
| `unicode` | `utf_ansi.c`, UTF-8/UTF-16 conversion, filename handling |
| `radio` | `http.c`, stream/ICY handling, shoutcast/icecast info |
| `resampler` | `resample.c` (`speex_resampler_*`) |
| `win9x` | Windows 9x/NT4/XP compatibility work, `wspiapi.c`, `isNT` paths |
| `deps` | `opusfile/` and the external libopus/libogg/opusfile builds |
| `build` | `Makefile*`, `makelib.bat`, compile/link flags |
| `release` | version bumps, tag preparation, changelog header |
| `docs` | readme, `in_opus.txt` prose, agent docs |

## Examples

```text
fix(unicode): reject overlong UTF-8 sequences when converting tags

The converter accepted 3-byte overlong encodings, which produced wrong
wide characters on NT4 and could read past the input buffer.

Closes #14
```

```text
feat(tags): add FORMAT_TITLE option for Winamp 2.x playlist entries

Winamp 2.x shows only the filename in the playlist. When FORMAT_TITLE=1
the plugin reports "Artist - Title" instead; setting it to 0 keeps the
old behavior.
```

```text
perf(build): enable -ffast-math and LTO partitioning for release builds
build(build): drop -fipa-pta from the GCC 5 makefile
chore(release): bump version to 0.914
docs(docs): document the IN_OPUS options in in_opus.txt
```

## Repository-specific notes

- Version bumps use `chore(release): bump version to X.YZ` and are tagged
  `vX.YZ` (see `.opencode/skills/in-opus-build/references/release-checklist.md`).
- Historical commits in this repository do not follow the convention. Do not
  rewrite or reformat them; the rule applies to new commits only.
- A merge commit produced by GitHub keeps its default
  `Merge pull request #N from …` subject. Do not reword it.
