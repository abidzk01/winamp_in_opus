#ifndef TAGWRITE_H
#define TAGWRITE_H

#include <stdio.h>

/* One pending tag change.
 * key is a Vorbis comment field name (compared case-insensitively).
 * val is the new UTF-8 value; NULL or "" deletes every field with that key.
 */
typedef struct {
    const char *key;
    const char *val;
} OpusTagEdit;

/* Rewrite the Vorbis comment header of an Ogg Opus file, applying edits.
 * fn8 is the file name in UTF-8, as Winamp hands it to the extended file info
 * API. Comments that are not edited are preserved byte for byte, together with
 * the vendor string, the trailing padding and any embedded
 * METADATA_BLOCK_PICTURE. Only the Ogg pages that carry the header packets are
 * rebuilt; the audio pages are copied with a shifted page sequence number and
 * a recomputed CRC.
 * Returns 1 on success. On failure the original file is left untouched and 0
 * is returned.
 */
int opus_write_tags(const char *fn8, const OpusTagEdit *edits, int n_edits);

/* fopen() for a name that may only be representable through the wide API. */
FILE *opus_open_file(const char *name8, const char *mode);

#endif
