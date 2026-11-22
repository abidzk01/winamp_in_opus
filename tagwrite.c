/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * tagwrite.c                                                              *
 *                                                                         *
 * Rewrites the Vorbis comment header (the OpusTags packet) of an Ogg Opus *
 * file, which is what the Winamp "extended file info" write API needs.    *
 *                                                                         *
 * The stream is re-muxed: OpusHead and OpusTags are written again as new   *
 * Ogg pages, then every following page is copied with a shifted page       *
 * sequence number and a recomputed CRC32. Audio packets are never touched, *
 * and comments that were not edited (including the vendor string, the      *
 * trailing padding and any embedded METADATA_BLOCK_PICTURE) are kept byte  *
 * for byte.                                                               *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "tagwrite.h"

/* Provided by utf_ansi.c, and already used that way by opusfile/stream.c. */
wchar_t *utf8_to_utf16(const char *utfs);

#define OGG_PAGE_HDR    27
#define OGG_MAX_SEGS    255
#define OGG_MAX_BODY    (255 * 255)          /* 65025: biggest page payload */
#define COPY_CHUNK      2048                 /* keep the stack small */
#define HEAD_MAX        4096
#define TAGS_MAX        (64u * 1024u * 1024u)/* give up on absurd comment headers */

#define FLAG_CONTINUED  0x01
#define FLAG_BOS        0x02
#define FLAG_EOS        0x04

#define GRANULE_UNKNOWN 0xFFFFFFFFFFFFFFFFULL

/****************************************************************************
 *  CRC 32, as Ogg defines it: poly 0x04C11DB7, init 0, no final xor, msb     *
 *  first.                                                                   *
 ****************************************************************************/
static unsigned int crc_tab[256];
static char crc_tab_ready = 0;

static void crc_setup(void)
{
    unsigned int i, j, r;

    for (i = 0; i < 256; i++) {
        r = i << 24;
        for (j = 0; j < 8; j++)
            r = (r & 0x80000000u) ? ((r << 1) ^ 0x04C11DB7u) : (r << 1);
        crc_tab[i] = r;
    }
    crc_tab_ready = 1;
}

static unsigned int crc_update(unsigned int crc, const unsigned char *buf, size_t len)
{
    if (!crc_tab_ready) crc_setup();
    while (len--) crc = (crc << 8) ^ crc_tab[((crc >> 24) & 0xFF) ^ *buf++];
    return crc;
}

/****************************************************************************
 *  Small helpers                                                           *
 ****************************************************************************/
static unsigned int get_le32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned long long get_le64(const unsigned char *p)
{
    return (unsigned long long)get_le32(p)
         | ((unsigned long long)get_le32(p + 4) << 32);
}

static void put_le32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void put_le64(unsigned char *p, unsigned long long v)
{
    put_le32(p, (unsigned int)v);
    put_le32(p + 4, (unsigned int)(v >> 32));
}

/* Case insensitive compare of a comment field name against a plain string. */
static int key_equal(const unsigned char *key, size_t key_len, const char *name)
{
    size_t i;

    if (name == NULL || strlen(name) != key_len) return 0;
    for (i = 0; i < key_len; i++) {
        unsigned char a = key[i];
        unsigned char b = (unsigned char)name[i];
        if (a >= 'a' && a <= 'z') a = (unsigned char)(a - 32);
        if (b >= 'a' && b <= 'z') b = (unsigned char)(b - 32);
        if (a != b) return 0;
    }
    return 1;
}

static char *name_with_ext(const char *name, const char *ext)
{
    size_t n = strlen(name), m = strlen(ext);
    char *p = (char *)malloc(n + m + 1);

    if (!p) return NULL;
    memcpy(p, name, n);
    memcpy(p + n, ext, m + 1);
    return p;
}

/* fopen() that also copes with names the ANSI code page cannot express. This
   follows opusfile/stream.c, which does the same for playback. */
FILE *opus_open_file(const char *name, const char *mode)
{
    FILE *fp = fopen(name, mode);

    if (!fp) {
        HINSTANCE hMsvcrtDll = GetModuleHandle("MSVCRT.DLL");
        FILE *(WINAPI *my_wfopen)(const wchar_t *, const wchar_t *) = NULL;

        if (!hMsvcrtDll) return NULL;
        my_wfopen = (FILE *(WINAPI *)(const wchar_t *, const wchar_t *))
                        (void *)GetProcAddress(hMsvcrtDll, "_wfopen");
        if (my_wfopen) {
            wchar_t *wname = utf8_to_utf16(name);
            wchar_t *wmode = utf8_to_utf16(mode);
            if (wname && wmode) fp = my_wfopen(wname, wmode);
            free(wname);
            free(wmode);
        }
    }
    return fp;
}

/* rename()/remove() that also cope with names the ANSI code page cannot
   express, using the same msvcrt fallback as opus_open_file(). */
static int remove_unicode(const char *name)
{
    HINSTANCE hMsvcrtDll;
    int (__cdecl *my_wremove)(const wchar_t *);
    wchar_t *wname;
    int r;

    if (remove(name) == 0) return 0;
    hMsvcrtDll = GetModuleHandle("MSVCRT.DLL");
    if (!hMsvcrtDll) return -1;
    my_wremove = (int (__cdecl *)(const wchar_t *))
                    (void *)GetProcAddress(hMsvcrtDll, "_wremove");
    if (!my_wremove) return -1;
    wname = utf8_to_utf16(name);
    if (!wname) return -1;
    r = my_wremove(wname);
    free(wname);
    return r;
}

static int rename_unicode(const char *from, const char *to)
{
    HINSTANCE hMsvcrtDll;
    int (__cdecl *my_wrename)(const wchar_t *, const wchar_t *);
    wchar_t *wfrom, *wto;
    int r;

    if (rename(from, to) == 0) return 0;
    hMsvcrtDll = GetModuleHandle("MSVCRT.DLL");
    if (!hMsvcrtDll) return -1;
    my_wrename = (int (__cdecl *)(const wchar_t *, const wchar_t *))
                    (void *)GetProcAddress(hMsvcrtDll, "_wrename");
    if (!my_wrename) return -1;
    wfrom = utf8_to_utf16(from);
    wto = utf8_to_utf16(to);
    if (!wfrom || !wto) {
        free(wfrom);
        free(wto);
        return -1;
    }
    r = my_wrename(wfrom, wto);
    free(wfrom);
    free(wto);
    return r;
}

static size_t page_body_len(const unsigned char *hdr, int nseg)
{
    size_t n = 0;
    int i;

    for (i = 0; i < nseg; i++) n += hdr[OGG_PAGE_HDR + i];
    return n;
}

/* Read one page header (27 bytes plus its lacing table) into hdr.
   1 = ok, 0 = clean end of file, -1 = malformed/truncated. */
static int read_page_hdr(FILE *f, unsigned char *hdr, int *nseg)
{
    size_t got = fread(hdr, 1, OGG_PAGE_HDR, f);

    if (got == 0) return 0;
    if (got != OGG_PAGE_HDR) return -1;
    if (memcmp(hdr, "OggS", 4) != 0 || hdr[4] != 0) return -1;

    *nseg = hdr[26];
    if (fread(hdr + OGG_PAGE_HDR, 1, (size_t)*nseg, f) != (size_t)*nseg) return -1;
    return 1;
}

/* Write one page whose payload is already in memory. */
static int write_page(FILE *out, int htype, unsigned long long granule,
                      unsigned int serial, unsigned int seq,
                      const unsigned char *seg, int nseg,
                      const unsigned char *body, size_t body_len)
{
    unsigned char hdr[OGG_PAGE_HDR + OGG_MAX_SEGS];
    unsigned int crc;
    int i;

    memcpy(hdr, "OggS", 4);
    hdr[4] = 0;
    hdr[5] = (unsigned char)htype;
    put_le64(hdr + 6, granule);
    put_le32(hdr + 14, serial);
    put_le32(hdr + 18, seq);
    put_le32(hdr + 22, 0);
    hdr[26] = (unsigned char)nseg;
    for (i = 0; i < nseg; i++) hdr[OGG_PAGE_HDR + i] = seg[i];

    crc = crc_update(0, hdr, (size_t)(OGG_PAGE_HDR + nseg));
    crc = crc_update(crc, body, body_len);
    put_le32(hdr + 22, crc);

    if (fwrite(hdr, 1, (size_t)(OGG_PAGE_HDR + nseg), out) != (size_t)(OGG_PAGE_HDR + nseg))
        return 0;
    if (body_len && fwrite(body, 1, body_len, out) != body_len) return 0;
    return 1;
}

/* Copy the payload of an already read page, shifting its sequence number and
   recomputing its CRC. The CRC field is patched once the payload is written. */
static int copy_page(FILE *in, FILE *out, unsigned char *hdr, int nseg,
                     unsigned long long body_len, unsigned int delta)
{
    unsigned char chunk[COPY_CHUNK];
    unsigned long long left = body_len;
    unsigned int crc;
    long patch;

    put_le32(hdr + 18, get_le32(hdr + 18) + delta);
    put_le32(hdr + 22, 0);
    crc = crc_update(0, hdr, (size_t)(OGG_PAGE_HDR + nseg));

    patch = ftell(out);
    if (patch < 0) return 0;
    if (fwrite(hdr, 1, (size_t)(OGG_PAGE_HDR + nseg), out) != (size_t)(OGG_PAGE_HDR + nseg))
        return 0;

    while (left) {
        size_t want = (left > COPY_CHUNK) ? COPY_CHUNK : (size_t)left;
        if (fread(chunk, 1, want, in) != want) return 0;
        crc = crc_update(crc, chunk, want);
        if (fwrite(chunk, 1, want, out) != want) return 0;
        left -= want;
    }

    put_le32(hdr + 22, crc);
    if (fseek(out, patch + 22, SEEK_SET) != 0) return 0;
    if (fwrite(hdr + 22, 1, 4, out) != 4) return 0;
    if (fseek(out, 0, SEEK_END) != 0) return 0;
    return 1;
}

static int grow_buf(unsigned char **buf, size_t *cap, size_t need)
{
    size_t ncap = *cap ? *cap : 8192;
    unsigned char *p;

    while (ncap < need) {
        if (ncap > TAGS_MAX / 2) return 0;
        ncap *= 2;
    }
    p = (unsigned char *)realloc(*buf, ncap);
    if (!p) return 0;
    *buf = p;
    *cap = ncap;
    return 1;
}

/* Split a header packet into Ogg pages and write them. Pages on which no
   packet ends carry a granule of -1, as Ogg requires; the page that ends the
   packet carries granule 0. Returns the next free sequence number, or 0. */
static unsigned int write_packet(FILE *out, const unsigned char *pkt, size_t pkt_len,
                                 unsigned int serial, unsigned int seq, int first_htype)
{
    unsigned char seg[OGG_MAX_SEGS];
    unsigned char terminator[1];
    size_t off = 0;
    int partial = 0;

    terminator[0] = 0;

    for (;;) {
        int nseg = 0, terminated = 0;
        size_t body_start = off, blen;

        while (nseg < OGG_MAX_SEGS) {
            if (off < pkt_len) {
                size_t n = pkt_len - off;
                if (n > 255) n = 255;
                seg[nseg++] = (unsigned char)n;
                off += n;
                if (n < 255) { terminated = 1; break; }
            } else {
                seg[nseg++] = 0;              /* terminates an exact multiple of 255 */
                terminated = 1;
                break;
            }
        }
        blen = off - body_start;

        if (!write_page(out, partial ? FLAG_CONTINUED : first_htype,
                        terminated ? 0 : GRANULE_UNKNOWN, serial, seq,
                        seg, nseg, pkt + body_start, blen))
            return 0;
        seq++;

        if (off >= pkt_len) {
            if (terminated) return seq;
            /* the packet ended on a page full of 255s: a lacing value of 0 on
               the following page terminates it */
            if (!write_page(out, FLAG_CONTINUED, 0, serial, seq,
                            terminator, 1, terminator, 0))
                return 0;
            return seq + 1;
        }
        partial = 1;
    }
}

/****************************************************************************
 *  Vorbis comment list                                                     *
 ****************************************************************************/
typedef struct {
    const unsigned char *field;  /* the whole "KEY=value" bytes */
    size_t field_len;
    const unsigned char *key;    /* field .. field + key_len */
    size_t key_len;
    const unsigned char *val;    /* NULL when the field has no '=' */
    size_t val_len;
} TagField;

/* Parse the OpusTags packet. vendor, tail and every field point inside pkt. */
static int split_tags(const unsigned char *pkt, size_t len,
                      const unsigned char **vendor, size_t *vendor_len,
                      TagField **fields, size_t *n_fields,
                      const unsigned char **tail, size_t *tail_len)
{
    size_t p = 8, i, n, vlen;
    TagField *f;

    if (len < 16 || memcmp(pkt, "OpusTags", 8) != 0) return 0;

    vlen = get_le32(pkt + p); p += 4;
    if (p + vlen + 4 > len) return 0;
    *vendor = pkt + p;
    *vendor_len = vlen;
    p += vlen;

    n = get_le32(pkt + p); p += 4;
    if (n > (len - p) / 4) return 0;          /* each field needs 4 bytes of length */

    f = (TagField *)calloc(n ? n : 1, sizeof(*f));
    if (!f) return 0;

    for (i = 0; i < n; i++) {
        size_t flen, k;

        if (p + 4 > len) { free(f); return 0; }
        flen = get_le32(pkt + p); p += 4;
        if (p + flen > len) { free(f); return 0; }

        f[i].field = pkt + p;
        f[i].field_len = flen;
        f[i].key = pkt + p;
        f[i].key_len = flen;
        f[i].val = NULL;
        f[i].val_len = 0;

        for (k = 0; k < flen; k++) {
            if (pkt[p + k] == '=') {
                f[i].key_len = k;
                f[i].val = pkt + p + k + 1;
                f[i].val_len = flen - k - 1;
                break;
            }
        }
        p += flen;
    }

    *fields = f;
    *n_fields = n;
    *tail = pkt + p;
    *tail_len = len - p;
    return 1;
}

static const OpusTagEdit *find_edit(const TagField *f, const OpusTagEdit *edits, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        if (edits[i].key && key_equal(f->key, f->key_len, edits[i].key)) return &edits[i];
    }
    return NULL;
}

/* An edit sets a value when val is not NULL and not empty; anything else asks
   for the field to be removed. */
static int edit_sets(const OpusTagEdit *e)
{
    return e != NULL && e->key != NULL && e->val != NULL && e->val[0] != '\0';
}

/* Does any field of the file already use this key? */
static int key_present(const TagField *fields, size_t n_fields, const char *key)
{
    size_t j;

    for (j = 0; j < n_fields; j++) {
        if (key_equal(fields[j].key, fields[j].key_len, key)) return 1;
    }
    return 0;
}

/* Build the new OpusTags packet: untouched fields byte for byte, edited fields
   as key=value, deleted fields dropped, new fields appended. */
static int build_new_tags(const TagField *fields, size_t n_fields,
                          const unsigned char *vendor, size_t vendor_len,
                          const OpusTagEdit *edits, int n_edits,
                          const unsigned char *tail, size_t tail_len,
                          unsigned char **out, size_t *out_len)
{
    size_t need = 8 + 4 + vendor_len + 4 + tail_len;
    size_t i, pos;
    unsigned char *buf;
    int e;

    for (i = 0; i < n_fields; i++) {
        const OpusTagEdit *ed = find_edit(&fields[i], edits, n_edits);
        if (ed) {
            if (edit_sets(ed)) need += 4 + strlen(ed->key) + 1 + strlen(ed->val);
        } else {
            need += 4 + fields[i].field_len;
        }
    }
    for (e = 0; e < n_edits; e++) {
        if (!edit_sets(&edits[e])) continue;
        if (!key_present(fields, n_fields, edits[e].key))
            need += 4 + strlen(edits[e].key) + 1 + strlen(edits[e].val);
    }
    if (need > TAGS_MAX) return 0;

    buf = (unsigned char *)malloc(need);
    if (!buf) return 0;

    memcpy(buf, "OpusTags", 8);
    pos = 8;
    put_le32(buf + pos, (unsigned int)vendor_len); pos += 4;
    if (vendor_len) memcpy(buf + pos, vendor, vendor_len);
    pos += vendor_len;

    put_le32(buf + pos, 0); pos += 4;         /* the comment count, patched below */

    for (i = 0; i < n_fields; i++) {
        const OpusTagEdit *ed = find_edit(&fields[i], edits, n_edits);

        if (!ed) {
            put_le32(buf + pos, (unsigned int)fields[i].field_len); pos += 4;
            memcpy(buf + pos, fields[i].field, fields[i].field_len);
            pos += fields[i].field_len;
        } else if (edit_sets(ed)) {
            size_t klen = strlen(ed->key), vlen = strlen(ed->val);
            put_le32(buf + pos, (unsigned int)(klen + 1 + vlen)); pos += 4;
            memcpy(buf + pos, ed->key, klen); pos += klen;
            buf[pos++] = '=';
            memcpy(buf + pos, ed->val, vlen); pos += vlen;
        }
    }

    {   size_t count = 0;
        for (i = 0; i < n_fields; i++) {
            const OpusTagEdit *ed = find_edit(&fields[i], edits, n_edits);
            if (ed && !edit_sets(ed)) continue;
            count++;
        }
        for (e = 0; e < n_edits; e++) {
            if (!edit_sets(&edits[e])) continue;
            if (key_present(fields, n_fields, edits[e].key)) continue;
            put_le32(buf + pos, (unsigned int)(strlen(edits[e].key) + 1 + strlen(edits[e].val)));
            pos += 4;
            memcpy(buf + pos, edits[e].key, strlen(edits[e].key)); pos += strlen(edits[e].key);
            buf[pos++] = '=';
            memcpy(buf + pos, edits[e].val, strlen(edits[e].val)); pos += strlen(edits[e].val);
            count++;
        }
        put_le32(buf + 12 + vendor_len, (unsigned int)count);
    }

    if (tail_len) memcpy(buf + pos, tail, tail_len);
    pos += tail_len;

    *out = buf;
    *out_len = pos;
    return 1;
}

/****************************************************************************
 *  Public entry point                                                      *
 ****************************************************************************/
int opus_write_tags(const char *fn8, const OpusTagEdit *edits, int n_edits)
{
    FILE *in = NULL, *out = NULL;
    char *tmpname = NULL, *bakname = NULL;
    unsigned char *hdr = NULL, *body = NULL, *leftover = NULL;
    unsigned char *head = NULL, *tags = NULL, *new_tags = NULL;
    unsigned char leftover_seg[OGG_MAX_SEGS];
    const unsigned char *vendor = NULL, *tail = NULL;
    size_t vendor_len = 0, tail_len = 0;
    size_t head_len = 0, head_cap = 0, tags_len = 0, tags_cap = 0, new_len = 0;
    size_t leftover_len = 0;
    TagField *fields = NULL;
    size_t n_fields = 0;
    unsigned int serial = 0, seq = 0, end_seq = 0, delta = 0;
    unsigned long long end_granule = 0;
    int end_eos = 0, packet = 0, done = 0, ret = 0, nseg = 0, i;
    int n_leftover_seg = 0;
    size_t blen;

    if (!fn8 || !*fn8) return 0;
    if (!edits || n_edits <= 0) return 1;    /* nothing to write */

    hdr = (unsigned char *)malloc(OGG_PAGE_HDR + OGG_MAX_SEGS);
    body = (unsigned char *)malloc(OGG_MAX_BODY + 1);
    tmpname = name_with_ext(fn8, ".tmp");
    bakname = name_with_ext(fn8, ".bak");
    if (!hdr || !body || !tmpname || !bakname) goto cleanup;

    in = opus_open_file(fn8, "rb");
    if (!in) goto cleanup;
    out = opus_open_file(tmpname, "wb");
    if (!out) goto cleanup;

    /**** collect the two header packets, page by page ****/
    while (!done) {
        size_t off = 0;
        int s;

        if (read_page_hdr(in, hdr, &nseg) != 1) goto cleanup;
        blen = page_body_len(hdr, nseg);
        if (fread(body, 1, blen, in) != blen) goto cleanup;

        if (packet == 0) {
            serial = get_le32(hdr + 14);
            if (!(hdr[5] & FLAG_BOS)) goto cleanup;
        }

        for (s = 0; s < nseg; s++) {
            size_t l = hdr[OGG_PAGE_HDR + s];

            if (packet == 0) {                     /* inside OpusHead */
                if (head_len + l > HEAD_MAX) goto cleanup;
                if (!grow_buf(&head, &head_cap, head_len + l + 1)) goto cleanup;
                memcpy(head + head_len, body + off, l);
                head_len += l;
            } else if (packet == 1) {               /* inside OpusTags */
                if (tags_len + l > TAGS_MAX) goto cleanup;
                if (!grow_buf(&tags, &tags_cap, tags_len + l + 1)) goto cleanup;
                memcpy(tags + tags_len, body + off, l);
                tags_len += l;
            }
            off += l;

            if (l < 255) {                         /* a packet ends here */
                packet++;
                if (packet == 2) {                 /* OpusTags is complete */
                    end_seq = get_le32(hdr + 18);
                    end_granule = get_le64(hdr + 6);
                    end_eos = (hdr[5] & FLAG_EOS) != 0;
                    n_leftover_seg = nseg - s - 1;
                    for (i = 0; i < n_leftover_seg && i < OGG_MAX_SEGS; i++)
                        leftover_seg[i] = hdr[OGG_PAGE_HDR + s + 1 + i];
                    leftover_len = page_body_len(hdr, nseg) - off;
                    if (leftover_len) {
                        leftover = (unsigned char *)malloc(leftover_len);
                        if (!leftover) goto cleanup;
                        memcpy(leftover, body + off, leftover_len);
                    }
                    done = 1;
                    break;
                }
            }
        }
    }

    if (head_len < 19 || !head || memcmp(head, "OpusHead", 8) != 0) goto cleanup;
    if (tags_len < 16 || !tags || memcmp(tags, "OpusTags", 8) != 0) goto cleanup;

    /**** rebuild the comment packet ****/
    if (!split_tags(tags, tags_len, &vendor, &vendor_len, &fields, &n_fields, &tail, &tail_len))
        goto cleanup;
    if (!build_new_tags(fields, n_fields, vendor, vendor_len, edits, n_edits,
                        tail, tail_len, &new_tags, &new_len))
        goto cleanup;

    /**** write the new stream ****/
    seq = write_packet(out, head, head_len, serial, 0, FLAG_BOS);
    if (!seq) goto cleanup;
    seq = write_packet(out, new_tags, new_len, serial, seq, 0);
    if (!seq) goto cleanup;

    if (n_leftover_seg) {                       /* the tail of the old header page */
        if (!write_page(out, end_eos ? FLAG_EOS : 0, end_granule, serial, seq,
                        leftover_seg, n_leftover_seg, leftover, leftover_len))
            goto cleanup;
        seq++;
    }

    delta = seq - (end_seq + 1);

    for (;;) {                                  /* copy the audio pages */
        int st = read_page_hdr(in, hdr, &nseg);
        if (st == 0) break;
        if (st < 0) goto cleanup;
        blen = page_body_len(hdr, nseg);
        if (!copy_page(in, out, hdr, nseg, blen, delta)) goto cleanup;
    }

    if (fflush(out) != 0) goto cleanup;
    if (fclose(out) != 0) { out = NULL; goto cleanup; }
    out = NULL;
    fclose(in);
    in = NULL;

    /**** swap the new file in, keeping the original until it is safe ****/
    remove_unicode(bakname);
    if (rename_unicode(fn8, bakname) != 0) goto cleanup;
    if (rename_unicode(tmpname, fn8) != 0) {
        rename_unicode(bakname, fn8);           /* put the original back */
        goto cleanup;
    }
    remove_unicode(bakname);
    ret = 1;

cleanup:
    if (out) fclose(out);
    if (in) fclose(in);
    if (!ret && tmpname) remove_unicode(tmpname); /* never leave a half written file */
    free(hdr);
    free(body);
    free(head);
    free(tags);
    free(new_tags);
    free(leftover);
    free(fields);
    free(tmpname);
    free(bakname);
    return ret;
}
