# Manual verification matrix

There is no automated test suite. Verify the rows relevant to the change and
state which rows you actually ran.

## Test material

- `.opus` files: 44.1 kHz stereo, 48 kHz stereo, mono voice, a very long file
  (> 1 h) for the HOURS_MODE path, a file with a huge tag block.
- `.opu` file (the alternate extension), plus a deliberately misnamed file
  (`.opus` extension containing an Ogg Vorbis stream) to check detection.
- A file with UTF-8 tags beyond Latin-1 (Cyrillic, Greek, CJK, emoji) to exercise
  `utf_ansi.c`, and a filename with non-ANSI characters.
- A file with and without ReplayGain tags (album + track gain, peak).
- HTTP Opus radio stream (a URL not ending in `.opus`, to check the `>.opus`
  hint), and a stream with Shoutcast/Icecast metadata.

## Rows

| # | Area | Check | Windows |
| - | ---- | ----- | ------- |
| 1 | Local playback | file plays, no glitch at start/end, correct duration in the playlist | all |
| 2 | Seek | scrub, seek backwards/forwards repeatedly, seek during pause, seek near EOF | all |
| 3 | Pause/stop | pause, resume, stop, play the next file immediately | all |
| 4 | Format detection | `isourfile` accepts `.opus`/`.opu`, rejects other files | all |
| 5 | Tags (Alt+3) | Title/Artist/Album/Date/Track/Genre/Composer/Performer/URL/Description shown; long values truncated safely | all |
| 6 | Unicode | non-Latin tags and filenames correct; on Win9x they fall back to the local codepage without garbling | 9x + NT-class |
| 7 | ReplayGain | album/track/auto/raw modes, `PRE_GAIN`, `TARGET_LUFS`, no clipping distortion | all |
| 8 | Output rates | `OUT_SAMPLERATE` at 8/22.05/44.1/48/96/192 kHz, `OUT_BITS` at 8/16/24/32 | all |
| 9 | Volume | internal volume on/off, Winamp volume slider, DSP plug-in pitch change while playing | all |
| 10 | Radio | stream opens, plays, Alt+3 shows server info, drop/reconnect handled | all |
| 11 | Config | every documented `[IN_OPUS]` key accepted, defaults when absent, `config()` text matches parsed keys | all |
| 12 | Hosts | Winamp 2.x (incl. `FORMAT_TITLE`), Winamp 5.x (incl. unified dialog), WACUP, XMPlay, MediaMonkey | all |

## Platforms

Windows 95 / 98SE / NT4 / 2000 / XP / 2003 / 7 / 8 / 10, 32-bit. Any change to
`wspiapi.c`, `utf_ansi.c`, allocation patterns, or API usage needs at least one
9x or NT4 run, not only a modern one.

## Reporting

In the commit body or PR description list: the platforms tested, the hosts used,
the file/stream types used, and anything not verified. If no player or no
toolchain was available, say the change is unverified — do not infer a result.
