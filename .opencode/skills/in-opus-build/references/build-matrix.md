# Build matrix

All build files live at the repository root. None of the third-party libraries
are vendored except `opusfile/`.

| Makefile | Toolchain | Key flags | Links | Purpose |
| -------- | --------- | --------- | ----- | ------- |
| `Makefile` | current MinGW-w64 (GCC 13.2 / winlibs) | `-m32 -Os -flto -flto-partition=none -march=i386 -ffast-math -fmerge-all-constants -fno-plt -Wstack-usage=4096` | `oflto152/*.o`, `opusfile/*.c`, `logg/*.c`, `-nostdlib -lgcc -lm -lkernel32 -lmsvcrt -luser32 -lgdi32 -lwsock32` | release `in_opus.dll`, smallest/fastest output |
| `Makefiledb` | same | `-Og -g -fanalyzer -march=i486 -DLOCAL_LRINTF`, no LTO | `libopus152.a`, `logg/*.c`, `opusfile/*.c` (no `-s`, keeps symbols) | debugging and static analysis |
| `MakefileGCC5` | TDM-GCC 5.1 (2015) | `-Os -flto -march=i486`, no `-ffast-math`, no `-fno-plt` | `oflto/*.o`, `logg/*.c` | keeps the plugin buildable with the legacy toolchain |
| `MakefiledbGCC5` | TDM-GCC 5.1 | `-Og -g -march=i486` | `libopus152.a`, `logg/*.c` | legacy debug build |
| `opusfile/makelib.bat` | any 32-bit GCC | `-Os -fno-stack-check -fno-stack-protector -mno-stack-arg-probe -march=i486 -mpreferred-stack-boundary=2` | compiles `opusfile/*.c` in place | rebuild the vendored, locally modified libopusfile 0.12 objects |

Targets common to the makefiles: `in_opus.dll`, the object files
(`in_opus.o`, `resample.o`, `infobox.o`, `http.o`, `wspiapi.o`, `resource.o`) and
`clean` (`rm *.o in_opus.dll`).

## Prerequisites (not in the repository)

`scripts/build-deps.sh` builds all of this in one go (libopus 1.5.2 and libogg
1.3.2), so a fresh clone only needs a 32-bit GCC, `windres`, `make`, `curl` and
`tar`:

- 32-bit GCC plus `windres` and `make` on `PATH`.
- `opus/` include directory containing `opusfile.h` (and its `opus/` deps) —
  referenced as `#include <opus/opusfile.h>` with `-I./opus`.
- Prebuilt libopus 1.5.2 + libogg 1.3.2 objects:
  - `oflto152/*.o` for the release build (LTO objects),
  - `oflto/*.o` for the GCC 5 build,
  - `libopus152.a` for the debug builds,
  - `logg/*.c` (libogg sources) compiled into the DLL.
- `resource.o` is produced by `windres resource.rc resource.o`.

The script fills `oflto152/`, `logg/`, `opus/` and `ogg/`; it does not produce
`oflto/*.o` or `libopus152.a`, which only the GCC 5 and debug makefiles need.

## Common failures

| Symptom | Cause / fix |
| ------- | ----------- |
| `cp: cannot create regular file 'D:\Program Files\...'` | Expected on any machine that is not the author's: the DLL is already built. Ignore, or delete the `cp` lines from the `in_opus.dll` recipe. |
| `cannot find -lopus` / undefined `op_*` symbols | The external libopus/libogg objects are missing or not the ones the makefile expects. |
| `opus/opusfile.h: No such file or directory` | The external `opus/` include tree is missing (`-I./opus`). |
| undefined reference to `_DllMain@12` | The entry point flag was dropped, or `DllMain` was renamed/changed. |
| `windres: command not found` | `resource.o` cannot be built without the MinGW resource compiler. |
| link errors mentioning `-m32` | A 64-bit toolchain or the wrong object format (`.a` vs LTO `.o`) is in use. |
| `dlltool`/import library errors for `wsock32` | The legacy network shim depends on `-lwsock32`; do not remove it. |
