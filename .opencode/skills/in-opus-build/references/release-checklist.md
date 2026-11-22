# Release checklist

Work top to bottom. Do not tag before the build is verified and the tree is
clean. All commands are run from the repository root.

## 1. Decide and record the version

- Current version: **v0.914** (`0,9,1,4`). Earlier tags: `v0.911`, `v0.912`,
  `v0.913`, `v0.914` (plus a combined `v0.333-v0.892` tag).
- Versioning is `0.MINOR` for the public name (`v0.915`) and
  `0,9,<minor>,0` for the resource version (`0,9,1,5`). A patch that does not
  change behavior can still get its own minor number — match the existing style.
- Confirm the new version with the user before editing files.

## 2. Bump every version location

| File | What to change |
| ---- | -------------- |
| `resource.rc` | `#define OPUS_VERSION 0,9,1,5` **and** both `"0,9,1,4"` strings (`FileVersion`, `ProductVersion`) |
| `in_opus.c` | the version in `about()` (`"OPUS File Player v0.914, ..."`) and in the `In_Module` name (`"OPUS Player v0.914 by Raymond"`) |
| `in_opus.txt` | the banner (`OPUS FILE DECODER PLUGIN FOR WINAMP 2+ (V0.915)`) and its release date line |
| `README.md` | only when features, limitations or the dependency list changed |

```sh
grep -rn "0\.914\|0,9,1,4" . --include="*.c" --include="*.rc" --include="*.txt" --include="*.md"
```

## 3. Changelog

Add a new section directly under the `== CHANGELOG ==` header in `in_opus.txt`,
above the previous release:

```text
== UPDATE 0.915 ==

* FIXED: <what the user saw> (<cause, if it explains the fix>)

* NEW: <capability, plus the option name and its default>

* CHANGED: <behavior change and how to restore the old behavior>
```

Keep entries user-facing and English-only, and mention new `[IN_OPUS]` options
with their values (see the existing `FORMAT_TITLE`, `UNIFIED_DIALOG` entries).

## 4. Build and verify

```sh
make -f Makefiledb     # warnings + -fanalyzer, must be clean
make                   # release in_opus.dll
```

- Confirm `in_opus.dll` is regenerated and the copies into
  `D:\Program Files\MediaPlayers\...` behave as expected for this machine.
- Run the release-relevant rows of `references/test-matrix.md`, especially the
  features and platforms touched by the changes.
- Check the DLL version in the file properties if the release includes
  `resource.rc` changes.

## 5. Commit, tag, publish

```sh
git status                 # no *.o, *.res, *.a, *.dll staged
git add resource.rc in_opus.c in_opus.txt README.md
git commit -m "chore(release): bump version to 0.915"
git tag -a v0.915 -m "v0.915"
git push origin main --follow-tags   # only when the user asks
```

- Release notes on GitHub summarise the changelog; keep them in English.
- Attach `in_opus.dll` (and `in_opus.txt`, which is the distributable readme) to
  the release; never commit those artifacts to the repository.
