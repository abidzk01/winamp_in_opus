---
name: in_opus build and release
description: Build, debug, and release the in_opus Winamp Opus input plugin (32-bit MinGW DLL). Use when asked to build in_opus, fix a compile/link error in it, add or change an IN_OPUS option, rebuild the vendored libopusfile, bump the plugin version, cut a vX.YZ release, or update the changelog in in_opus.txt.
---

# in_opus build and release

This repository is a Winamp 2.x/5.x input plugin (`In_Module` DLL) for Ogg Opus
files and HTTP Opus radio streams. It builds with MinGW as a 32-bit DLL and must
keep working on Windows 95 … 10. See the repository `AGENTS.md` for the
architecture and `.opencode/rules/30-c-conventions.md` for coding rules.

## Workflow

1. **Locate and read before changing.** The sources are at the repository root;
   the main file is `in_opus.c`. Check `.opencode/rules/` first, and read the
   references below when the task touches builds, versions, or verification.
2. **Make the change** following `.opencode/rules/30-c-conventions.md`
   (32-bit, no CRT additions, bounded string helpers, English comments).
3. **Build** from the repository root.
   - Analysis/debug build (do this first, it reports warnings and
     `-fanalyzer` findings): `make -f Makefiledb`
   - Release build: `make` → `in_opus.dll`
   - Legacy toolchain: `make -f MakefileGCC5`
   - Vendored libopusfile objects: `cd opusfile && ./makelib.bat`
   See `references/build-matrix.md` for what each makefile expects and for the
   prerequisite libraries (none of them are vendored).
4. **Report honestly.** If the toolchain or the prerequisite libraries are
   missing, say the build was not verified — never claim a build succeeded
   without running it. The `in_opus.dll` recipe also copies the DLL into the
   author's own `D:\Program Files\MediaPlayers\...` folders; failures there are
   non-fatal and expected on other machines.
5. **Verify behavior** with `references/test-matrix.md`. There is no automated
   test suite; anything touching decode, seek, radio, tags, replaygain or config
   parsing needs the relevant player/OS combination exercised, or an explicit
   statement that it was not.
6. **Commit** with Conventional Commits, in English (see
   `.opencode/rules/10-conventional-commits.md`), and only when the user asks.
7. **For a release**, work through `references/release-checklist.md` item by
   item — version bumps live in three files plus a tag.

## References

| File | Content |
| ---- | ------- |
| `references/build-matrix.md` | Makefile variants, flags, prerequisite libraries, common build failures |
| `references/release-checklist.md` | Version bump locations, changelog, README, tag, packaging |
| `references/test-matrix.md` | Manual verification matrix (players, OS, features, sample files) |

## Quick facts

- Version today: **v0.914**; resource version `0,9,1,4`; tags `v0.911` … `v0.914`.
- Version strings live in `resource.rc` (`OPUS_VERSION` + two strings),
  `in_opus.c` (`about()` and the `In_Module` name) and the banner of
  `in_opus.txt`.
- Options are read from `winamp.ini` / `in_opus.ini` under `[IN_OPUS]` by
  `readinternalconfig()`; `config()` only prints them, so adding an option means
  touching defaults, parsing, persistence and that text.
- Never commit `*.o`, `*.res`, `*.a`, `*.dll`, or the external library trees —
  `.gitignore` covers them.
