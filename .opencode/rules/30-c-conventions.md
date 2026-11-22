# Rule: C conventions for in_opus

The plugin is a 32-bit MinGW Windows DLL that must keep running on Windows 95,
98SE, NT4, 2000, XP, 2003, 7, 8 and 10, and must keep loading under Winamp 2.x
and 5.x, WACUP, XMPlay and MediaMonkey.

## Toolchain and language level

- Build as C with the flags in `Makefile` / `Makefiledb`: `-m32`, `-Wall`,
  `-Wextra`, `-Wstack-usage=4096`, and `-nostdlib -mdll -e_DllMain@12` when
  linking. `Makefiledb` additionally uses `-fanalyzer`.
- Keep the code compilable by both the release toolchain (MinGW-w64 GCC 13.x with
  LTO) and the legacy TDM-GCC 5 variants in `MakefileGCC5` / `MakefiledbGCC5`.
- Stay within C89/C99: no C11+ features, no C++ (no `//`-only flaws assumed, no
  `extern "C"` guards needed, no exceptions/RTTI), no GNU statement-expressions.
- Warnings are errors in practice: a change must not add any new warning, and
  `-fanalyzer` findings must be addressed or explained.
- Keep `DllMain` exactly as declared; the entry point is `_DllMain@12`.
- Keep the DLL size in mind (this project ships a tiny plugin): prefer the
  existing compact helpers over pulling in library code.

## Windows 9x / NT4 compatibility

- Assume the CRT is minimal and partly broken on 9x. Do not introduce CRT calls
  that are missing there (in particular wide-char helpers such as `wcslen`,
  `swprintf`, `wcs*` — the project implements UTF-8/UTF-16 conversion itself in
  `utf_ansi.c`).
- Do not rely on APIs newer than the oldest target. When a newer API is
  unavoidable, resolve it dynamically and keep a fallback path
  (`wspiapi.c`/`WspiapiGetAddrInfo` is the model); gate behavior on the existing
  runtime probes (`isNT`, `VIS_BROKEN`) rather than assuming a modern OS.
- Keep the binary a 32-bit DLL with no external DLL dependency beyond
  `kernel32`, `user32`, `gdi32`, `msvcrt` and `wsock32` (the link line in the
  Makefiles is the contract).

## Code style

- Match the surrounding style: 4-space indentation, K&R-style braces, `char` for
  small config flags, file-scope `static` for internal state, `//` or `/* */`
  comments only where they explain something non-obvious.
- Keep source files ASCII-only. No smart quotes, no Unicode identifiers, and no
  BOM. (`resource.rc` already contains one CP1252 byte in a caption — do not add
  more.)
- English comments only; add a comment when a fix is subtle (bounds math,
  encoding edge cases, Win9x workarounds), not for trivial lines.
- Public plugin entry points keep the names and signatures the Winamp
  `In_Module` struct expects (`config`, `about`, `init`, `quit`, `getfileinfo`,
  `infoDlg`, `isourfile`, `play`, `pause`, `unpause`, `ispaused`, `stop`,
  `setvolume`, …).

## Safety and robustness

- Prefer the bounded string helpers already in the project (`lstrcpy_sA`,
  `strcpy_s`, `snprintf` with explicit sizes) over unbounded `strcpy`/`sprintf`.
  Keep the "N" semantics explicit: the size argument includes the NUL.
- Validate sizes and capacities before copying or writing metadata
  (`cap`, `dmax`, `of->*`), and keep arithmetic from wrapping: the project has
  had repeated capacity-calculation bugs.
- No unchecked `malloc`/`realloc` result; free everything you allocate on every
  exit path (the decode thread's `QUIT:` label is the model).
- Treat every byte from a file, tag or network stream as untrusted input: check
  lengths before decoding, reject overlong UTF-8 sequences, and never trust a
  header length without bounds-checking it.
- Keep the decode thread and the main thread's shared state as it is today
  (`volatile seek_needed`, `killDecodeThread`); do not add new shared state
  without a clear synchronization story.
- Persist new options through the existing config path: defaults inside
  `readinternalconfig()`, read/write of `winamp.ini [IN_OPUS]`, and a line in the
  `config()` text listing every option.

## Upstream-derived files

`opusfile/*`, `in2.h`, `wa_ipc.h`, `http.c`, `resample.c`/`resample.h`,
`wspiapi.c`/`wspiapi.h` came from other projects. Change them only when required
for a fix or for Win9x compatibility, keep the upstream formatting, keep the
BSD/licence headers untouched, and describe the deviation in the commit body.
