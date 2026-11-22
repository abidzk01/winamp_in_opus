# AGENTS.md — in_opus

Guidance for AI agents working in this repository. Run OpenCode with this
directory (`in_opus`) as the working directory: `AGENTS.md`, `opencode.json` and
`.opencode/` live here, beside the sources they describe.

**Everything written for this project is in English**: code comments, docs,
commit messages, PR titles/descriptions, changelog entries, and any status text
about the project. See `.opencode/rules/00-language.md`.

## What this project is

This repository is a **Winamp 2.x/5.x input plugin DLL** (`In_Module`) that plays Ogg
Opus files (`*.opus`, `*.opu`) and Ogg Opus HTTP radio streams.

| Item | Value |
| ---- | ----- |
| Upstream | Raymond Gillibert — <https://github.com/RamonUnch/in_opus> |
| Current version | **v0.914** (resource version `0,9,1,4`) |
| Built on | libopus 1.5.2, libogg 1.3.2, libopusfile 0.12 (locally modified copy vendored in `opusfile/`) |
| Build output | a single 32-bit `in_opus.dll`, copied by the user into the player's `Plugins` folder. No installer, no extra runtime files. |
| Hosts | Winamp 2.x/5.x, WACUP, XMPlay 2.1+ (3.7+ for UNICODE_FILE), MediaMonkey 3.2.5/4.1.29 |
| OS | Windows 95/98SE/NT4/2000/XP/2003/7/8/10 |
| Tests / CI | none — verification is a manual matrix (see the `in-opus-build` skill) |

Compatibility with old Windows is the reason the project exists. It must keep
building as a 32-bit DLL and must not silently drop Windows 9x/NT4 support. When
an API is missing on old Windows the project either ships its own implementation
(`wspiapi.c`, `utf_ansi.c`) or falls back at runtime (`isNT`, `VIS_BROKEN`).

## Repository layout

| Path | Role |
| ---- | ---- |
| `in_opus.c` | Main plugin: `In_Module` definition, decode thread, seeking, replaygain, `winamp.ini [IN_OPUS]` options, file/stream detection |
| `infobox.c/.h` | "File info" (Alt+3) tag dialog; `#include "utf_ansi.c"` |
| `utf_ansi.c` | UTF-8 ⇄ UTF-16 conversion written for old Windows (no CRT wide-char calls) |
| `resample.c/.h` | Speex-derived resampler (`speex_resampler_*`) |
| `http.c/.h` | HTTP client derived from libopusfile's `http.c`, adapted for Win9x/NT (`OP_ENABLE_HTTP 1`) |
| `wspiapi.c/.h` | `WspiapiGetAddrInfo` style shim for Windows without `getaddrinfo` |
| `resource.rc`, `resource.h` | Dialogs and `VERSIONINFO` (`OPUS_VERSION`) |
| `in2.h`, `wa_ipc.h` | Winamp input-plugin SDK headers (vendored — do not reformat) |
| `opusfile/` | Vendored, locally modified libopusfile 0.12 sources (`makelib.bat`, `internal.h`, `stream.c`, `info.c`) |
| `Makefile` | Release build (current MinGW-w64, LTO objects from `oflto152/`) |
| `Makefiledb` | Debug/analysis build (`-Og -g -fanalyzer`, `libopus152.a`) |
| `MakefileGCC5`, `MakefiledbGCC5` | Legacy TDM-GCC 5 variants (`oflto/*.o`, `-march=i486`) |
| `in_opus.txt` | Distributable readme **that also carries the changelog** |
| `README.md` | Short project readme |

Agent configuration is part of this repository:

```text
AGENTS.md                     # this file
opencode.json                 # loads the rules below
.opencode/rules/*.md          # language, commit, git and C conventions
.opencode/skills/in-opus-build/
```

Discovery is upward-only: these files apply when OpenCode runs in `in_opus` (or
a directory below it). Started from a parent directory, only `AGENTS.md` is
picked up, and only once files here are read.

## Build

Prerequisites are **not vendored** and must exist beside the sources:

- a 32-bit MinGW toolchain (`gcc`, `windres`, `make`) — the author uses
  MinGW-w64/winlibs GCC 13.2 for the release build and TDM-GCC 5.1 for the
  legacy build;
- libopus + libogg + libopusfile built as the Makefile expects:
  `oflto152/*.o` (release, LTO) or `oflto/*.o` (GCC 5) or `libopus152.a`
  (debug), plus `logg/*.c` and an `opus/` include directory
  (`#include <opus/opusfile.h>`, `-I./opus`).

```sh
# release build (produces in_opus.dll)
make

# debug / static-analysis build (-Og -g -fanalyzer, links the .a lib)
make -f Makefiledb

# legacy GCC 5 build
make -f MakefileGCC5

# rebuild the vendored libopusfile objects
cd opusfile && ./makelib.bat

# remove local objects and the DLL
make clean
```

Notes:

- The `in_opus.dll` recipe ends with `cp` commands that install the DLL into the
  author's own player folders under `D:\Program Files\MediaPlayers\...`. On any
  other machine those copies fail **after** the DLL has been produced — that is
  expected, or delete those recipe lines.
- The DLL is linked with `-nostdlib`, `-mdll` and entry point `_DllMain@12`, so
  `DllMain` in `in_opus.c` must keep its exact `WINAPI` signature.
- Do not commit build artifacts: `.o`, `.res`, `.a`, `*.dll`, or the external
  library trees (`oflto*`, `logg`, `opus`). `.gitignore` covers them.
- If `gcc`/`make`/`windres` are not installed, say so instead of guessing: the
  build cannot be verified in this workspace (checked: not on `PATH`).

## Coding conventions

Full rules: `.opencode/rules/30-c-conventions.md`. The essentials:

- Plain C, 32-bit (`-m32`), compiled with `-Wall -Wextra -Wstack-usage=4096`;
  warnings must stay clean, and `Makefiledb` additionally runs `-fanalyzer`.
- Stay compatible with the oldest supported Windows and with `-nostdlib`
  linking. Avoid CRT functions that are missing or broken on Windows 9x
  (wide-char CRT helpers in particular) — use the project's own helpers.
- Prefer the existing bounded string helpers (`lstrcpy_sA`, `strcpy_s`,
  `snprintf` with explicit sizes) over unbounded `strcpy`/`sprintf`.
- Keep source files ASCII, keep the existing brace/indent style, and write new
  comments in English.
- `opusfile/`, `in2.h`, `wa_ipc.h`, `http.c`, `resample.c` and `wspiapi.c` come
  from upstream projects: touch them only when a fix requires it, keep the
  upstream style, and mention the change in the commit body.

## Versioning and release

A release touches three files and one tag:

1. `resource.rc` → `OPUS_VERSION` and the two `"0,9,1,4"` strings.
2. `in_opus.c` → version string in `about()` (line ~137) and in the
   `In_Module` name (line ~1095).
3. `in_opus.txt` → the header banner (`V0.914`) and a new
   `== UPDATE X.YZ ==` changelog section at the top of the changelog.
4. Commit as `chore(release): bump version to X.YZ`, then tag `vX.YZ`
   (existing tags: `v0.911` … `v0.914`).

`README.md` is updated when features or limitations change. Details:
`.opencode/skills/in-opus-build/references/release-checklist.md`.

## Verification

There is no automated test suite. Before claiming a change works:

- build with `make -f Makefiledb` for warnings and `-fanalyzer` findings;
- run the manual matrix in
  `.opencode/skills/in-opus-build/references/test-matrix.md` for anything that
  touches decoding, seeking, radio, tags, replaygain, or config parsing;
- if the change cannot be exercised (no Windows player, no toolchain), say
  explicitly that it was not verified.

## Git rules

`.opencode/rules/10-conventional-commits.md` and
`.opencode/rules/20-git-workflow.md`. Summary:

- **Conventional Commits, English only**: `type(scope): imperative subject`,
  plus a body/footer when useful.
- One logical change per commit; never commit build artifacts; don't commit or
  push unless asked.
- Never rewrite published history on `main`, never force-push, never amend
  pushed commits.
