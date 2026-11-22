/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * albumart.c                                                              *
 *                                                                         *
 * Album art provider for Winamp. Registers a service of the type           *
 * svc_albumArtProvider ('aap') so that Winamp's Album Art window and the   *
 * artwork pane of the File Info dialog can display the cover art that is   *
 * embedded in an Ogg Opus file as a METADATA_BLOCK_PICTURE comment.        *
 *                                                                         *
 * The Winamp SDK headers for these interfaces are not part of this project,*
 * so the pieces that are used are declared here. A Wasabi "Dispatchable" is *
 * an object whose only virtual method is                                 *
 *     int _dispatch(int msg, void *retval, void **params, int nparam)      *
 * where params[i] points at argument i and the return value (if any) is    *
 * written through retval. The method uses the x86 __thiscall convention,   *
 * which MinGW spells __attribute__((thiscall)).                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <opus/opusfile.h>

#include "albumart.h"
#include "tagwrite.h"
#include "wa_ipc.h"

/* Diagnostics. Set ART_LOG=1 in the [IN_OPUS] section of winamp.ini (or the
   plug-in's own ini) to have the provider append what Winamp asks for to
   %TEMP%\in_opus_art.log; that is the quickest way to find out whether the
   album art service of a particular Winamp build talks to the provider. */
extern char ART_LOG;

static void alog_write(const char *line)
{
    char path[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, path);
    FILE *f;

    if (n == 0 || n > MAX_PATH - 24) return;
    strcat(path, "in_opus_art.log");
    f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fclose(f);
}

/* No side effects inside ALOG: it is compiled away when ART_LOG is 0. */
#define ALOG(...)                                                       \
    do {                                                                \
        if (ART_LOG) {                                                  \
            char alog_buf_[512];                                        \
            sprintf(alog_buf_, __VA_ARGS__);                            \
            alog_write(alog_buf_);                                      \
        }                                                               \
    } while (0)
wchar_t *utf8_to_utf16(const char *utfs);   /* utf_ansi.c */
char *utf16_to_utf8(const wchar_t *in);     /* utf_ansi.c */

/****************************************************************************
 *  Wasabi Dispatchable ABI                                                 *
 ****************************************************************************/
typedef int (__attribute__((thiscall)) *WasabiDispatch)(void *self, int msg,
        void *retval, void **params, int nparam);

typedef struct {
    WasabiDispatch *vtbl;
} WasabiObj;

static int wasabi_call(void *obj, int msg, void *retval, void **params, int nparam)
{
    WasabiObj *o = (WasabiObj *)obj;

    if (!o || !o->vtbl || !o->vtbl[0]) return 0;
    return o->vtbl[0](obj, msg, retval, params, nparam);
}

/* api_service dispatch codes (Winamp SDK api_service.h) */
#define SVC_REGISTER            10
#define SVC_GETSERVICEBYGUID    50

/* waServiceFactory dispatch codes */
#define FACTORY_GETSERVICETYPE  100
#define FACTORY_GETSERVICENAME  200
#define FACTORY_GETGUID         210
#define FACTORY_GETINTERFACE    300
#define FACTORY_NONLOCKING      301
#define FACTORY_RELEASE         310
#define FACTORY_GETTESTSTRING   500
#define FACTORY_SERVICENOTIFY   600

/* svc_albumArtProvider dispatch codes */
#define AAP_PROVIDERTYPE        0
#define AAP_GETALBUMARTDATA     10
#define AAP_ISMINE              20
#define AAP_SETALBUMARTDATA     30
#define AAP_DELETEALBUMART      40

/* api_memmgr dispatch codes */
#define MEMMGR_SYSMALLOC        0

#define AAP_SERVICETYPE         0x00616170u   /* MK3CC('a','a','p') */
#define AAP_TYPE_EMBEDDED       0
#define AAP_SUCCESS             0
#define AAP_FAILURE             1

/* {in_opus album art provider} */
static const GUID in_opus_albumart_guid =
    { 0x7b1a5c44, 0x9f2e, 0x4d61, { 0x9c, 0x3a, 0x51, 0x8e, 0x6d, 0x2f, 0x40, 0x11 } };
/* {000CF46E-4DF6-4a43-BBE7-40E7A3EA02ED} from api_memmgr.h */
static const GUID memmgr_guid =
    { 0x000cf46e, 0x4df6, 0x4a43, { 0xbb, 0xe7, 0x40, 0xe7, 0xa3, 0xea, 0x02, 0xed } };

/****************************************************************************
 *  Interface call helpers                                                  *
 ****************************************************************************/
static void *g_svc = NULL;      /* api_service* */
static void *g_memmgr = NULL;   /* api_memmgr*  */

static int svc_register(void *svc, void *factory)
{
    int ret = 0;
    void *params[1];

    params[0] = &factory;
    wasabi_call(svc, SVC_REGISTER, &ret, params, 1);
    return ret;
}

static void *svc_by_guid(void *svc, const GUID *guid)
{
    void *ret = NULL;
    void *params[1];

    params[0] = (void *)guid;
    wasabi_call(svc, SVC_GETSERVICEBYGUID, &ret, params, 1);
    return ret;
}

static void *factory_get_interface(void *factory)
{
    void *ret = NULL;
    int lock = 1;
    void *params[1];

    params[0] = &lock;
    wasabi_call(factory, FACTORY_GETINTERFACE, &ret, params, 1);
    return ret;
}

/* The art data is handed to Winamp, which frees it with its own allocator, so
   use that allocator whenever the memory manager service is available. */
static void *art_alloc(size_t size)
{
    void *ret = NULL;
    void *params[1];

    params[0] = &size;
    if (g_memmgr) wasabi_call(g_memmgr, MEMMGR_SYSMALLOC, &ret, params, 1);
    if (!ret) ret = malloc(size);
    return ret;
}

/****************************************************************************
 *  Reading the embedded picture                                            *
 ****************************************************************************/
static int has_opus_ext(const wchar_t *p)
{
    size_t n;

    if (!p) return 0;
    n = wcslen(p);
    if (n < 4) return 0;
    p += n - 4;
    return !_wcsicmp(p, L".opu") || (n > 4 && !_wcsicmp(p - 1, L".opus"));
}

/* Winamp passes an album art "type" name; map it onto the picture type of the
   FLAC picture block. 3 is the front cover, which is the default. */
static int wanted_picture_type(const wchar_t *type)
{
    static const struct { const wchar_t *name; int type; } TYPES[] = {
        { L"fileicon",      1 }, { L"icon",          2 }, { L"cover",        3 },
        { L"back",          4 }, { L"leaflet",       5 }, { L"media",        6 },
        { L"leadartist",    7 }, { L"artist",        8 }, { L"conductor",    9 },
        { L"band",         10 }, { L"composer",     11 }, { L"lyricist",    12 },
        { L"location",     13 }, { L"recording",    14 }, { L"performance", 15 },
        { L"preview",      16 }, { L"fish",         17 }, { L"illustration",18 },
        { L"artistlogo",   19 }, { L"publisherlogo",20 }
    };
    size_t i;

    if (!type || !*type) return 3;
    for (i = 0; i < sizeof(TYPES) / sizeof(TYPES[0]); i++) {
        if (!_wcsicmp(type, TYPES[i].name)) return TYPES[i].type;
    }
    return 3;
}

static wchar_t *alloc_mime(const char *mime)
{
    wchar_t *w;
    size_t n, i;

    if (!mime || !*mime) mime = "image/jpeg";
    n = strlen(mime);
    w = (wchar_t *)art_alloc((n + 1) * sizeof(wchar_t));
    if (!w) return NULL;
    for (i = 0; i < n; i++) w[i] = (wchar_t)(unsigned char)mime[i];
    w[n] = 0;
    return w;
}

/* Hands back the picture data for the requested type, preferred order:
   the requested type, then the front cover, then whatever is stored. */
static int load_art(const wchar_t *fnw, const wchar_t *type,
                    void **bits, size_t *len, wchar_t **mime)
{
    OggOpusFile *of;
    const OpusTags *tags;
    OpusPictureTag pic;
    char *fn8;
    int err = 0, count, i, pass, want, ok = 0;

    if (!fnw || !bits || !len || !mime) return AAP_FAILURE;
    *bits = NULL;
    *len = 0;
    *mime = NULL;
    if (!has_opus_ext(fnw)) return AAP_FAILURE;

    fn8 = utf16_to_utf8(fnw);
    if (!fn8) return AAP_FAILURE;
    of = op_open_file(fn8, &err);
    free(fn8);
    if (!of) return AAP_FAILURE;

    tags = op_tags(of, -1);
    count = tags ? opus_tags_query_count(tags, "METADATA_BLOCK_PICTURE") : 0;
    if (count <= 0) {
        op_free(of);
        return AAP_FAILURE;
    }

    want = wanted_picture_type(type);
    ALOG("load_art: type=%ls want=%d count=%d", type ? type : L"(null)", want, count);
    for (pass = 0; pass < 3 && !ok; pass++) {
        int looking_for = (pass == 0) ? want : (pass == 1) ? 3 : -1;

        for (i = 0; i < count && !ok; i++) {
            const char *tag = opus_tags_query(tags, "METADATA_BLOCK_PICTURE", i);

            if (!tag) continue;
            opus_picture_tag_init(&pic);
            if (opus_picture_tag_parse(&pic, tag) >= 0
                && (looking_for < 0 || (int)pic.type == looking_for)
                && pic.data && pic.data_length) {
                void *copy = art_alloc(pic.data_length);
                wchar_t *wmime = alloc_mime(pic.mime_type);
                if (copy && wmime) {
                    memcpy(copy, pic.data, pic.data_length);
                    *bits = copy;
                    *len = pic.data_length;
                    *mime = wmime;
                    ok = 1;
                }
            }
            opus_picture_tag_clear(&pic);
        }
    }
    op_free(of);
    ALOG("load_art: result=%s bytes=%lu mime=%ls first=%02X%02X%02X%02X bits=%p",
         ok ? "ok" : "fail", (unsigned long)*len, (*mime && ok) ? *mime : L"(none)",
         (ok && *len > 3) ? ((unsigned char *)*bits)[0] : 0,
         (ok && *len > 3) ? ((unsigned char *)*bits)[1] : 0,
         (ok && *len > 3) ? ((unsigned char *)*bits)[2] : 0,
         (ok && *len > 3) ? ((unsigned char *)*bits)[3] : 0, *bits);
    return ok ? AAP_SUCCESS : AAP_FAILURE;
}

/****************************************************************************
 *  Writing / removing the embedded picture                                 *
 ****************************************************************************/
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64_encode(const unsigned char *in, size_t len)
{
    char *out;
    size_t i = 0, j = 0;

    out = (char *)malloc(((len + 2) / 3) * 4 + 1);
    if (!out) return NULL;

    while (i + 2 < len) {
        unsigned int v = ((unsigned int)in[i] << 16) | ((unsigned int)in[i + 1] << 8) | in[i + 2];
        out[j++] = B64[(v >> 18) & 0x3F];
        out[j++] = B64[(v >> 12) & 0x3F];
        out[j++] = B64[(v >> 6) & 0x3F];
        out[j++] = B64[v & 0x3F];
        i += 3;
    }
    if (i < len) {
        unsigned int v = (unsigned int)in[i] << 16;
        if (i + 1 < len) v |= (unsigned int)in[i + 1] << 8;
        out[j++] = B64[(v >> 18) & 0x3F];
        out[j++] = B64[(v >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? B64[(v >> 6) & 0x3F] : '=';
        out[j++] = '=';
    }
    out[j] = 0;
    return out;
}

static void put_be32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

/* Replaces the embedded artwork with the given image (FLAC picture block,
   stored base64 encoded in the METADATA_BLOCK_PICTURE comment). */
static int set_art(const wchar_t *fnw, const wchar_t *type, const void *bits,
                   size_t len, const wchar_t *mimew)
{
    char mime[128];
    unsigned char *block;
    char *b64, *fn8;
    OpusTagEdit edit;
    size_t mlen = 0, blen, p = 0, i;
    int ret;

    if (!fnw || !bits || !len || len > 32u * 1024u * 1024u) return AAP_FAILURE;
    if (!has_opus_ext(fnw)) return AAP_FAILURE;

    if (mimew && *mimew) {
        for (i = 0; mimew[i] && i < sizeof(mime) - 1; i++) mime[i] = (char)(unsigned char)mimew[i];
        mime[i] = 0;
        mlen = i;
    } else {
        strcpy(mime, "image/jpeg");
        mlen = 10;
    }

    blen = 4 + 4 + mlen + 4 + 16 + 4 + len;
    block = (unsigned char *)malloc(blen);
    if (!block) return AAP_FAILURE;

    put_be32(block + p, (unsigned int)wanted_picture_type(type)); p += 4;
    put_be32(block + p, (unsigned int)mlen); p += 4;
    memcpy(block + p, mime, mlen); p += mlen;
    put_be32(block + p, 0); p += 4;                 /* empty description */
    put_be32(block + p, 0); p += 4;                 /* width   */
    put_be32(block + p, 0); p += 4;                 /* height  */
    put_be32(block + p, 0); p += 4;                 /* depth   */
    put_be32(block + p, 0); p += 4;                 /* colours */
    put_be32(block + p, (unsigned int)len); p += 4;
    memcpy(block + p, bits, len);

    b64 = base64_encode(block, blen);
    free(block);
    if (!b64) return AAP_FAILURE;

    fn8 = utf16_to_utf8(fnw);
    if (!fn8) {
        free(b64);
        return AAP_FAILURE;
    }
    edit.key = "METADATA_BLOCK_PICTURE";
    edit.val = b64;
    ret = opus_write_tags(fn8, &edit, 1);
    free(b64);
    free(fn8);
    ALOG("set_art: %lu bytes -> %d", (unsigned long)len, ret);
    return ret ? AAP_SUCCESS : AAP_FAILURE;
}

/* Removes the embedded artwork. The tag writer works per comment field, so
   every embedded picture goes away, whatever its type. */
static int delete_art(const wchar_t *fnw)
{
    OpusTagEdit edit;
    char *fn8;
    int ret;

    if (!fnw || !has_opus_ext(fnw)) return AAP_FAILURE;
    fn8 = utf16_to_utf8(fnw);
    if (!fn8) return AAP_FAILURE;
    edit.key = "METADATA_BLOCK_PICTURE";
    edit.val = "";
    ret = opus_write_tags(fn8, &edit, 1);
    free(fn8);
    ALOG("delete_art -> %d", ret);
    return ret ? AAP_SUCCESS : AAP_FAILURE;
}

/****************************************************************************
 *  The provider and its service factory                                    *
 ****************************************************************************/
static WasabiDispatch provider_vtbl[1];
static WasabiDispatch factory_vtbl[1];
static WasabiObj g_provider_object;
static WasabiObj g_factory_object;

static int __attribute__((thiscall)) provider_dispatch(void *self, int msg,
        void *retval, void **params, int nparam)
{
    (void)self;

    ALOG("provider_dispatch msg=%d", msg);
    switch (msg) {
    case AAP_PROVIDERTYPE:
        if (retval) *(int *)retval = AAP_TYPE_EMBEDDED;
        return 1;

    case AAP_ISMINE:
        if (retval && params && nparam >= 1) {
            const wchar_t *fn = *(const wchar_t **)params[0];
            *(unsigned char *)retval = (unsigned char)(has_opus_ext(fn) ? 1 : 0);
        }
        return 1;

    case AAP_GETALBUMARTDATA:
        if (retval) {
            int r = AAP_FAILURE;
            if (params && nparam >= 5) {
                r = load_art(*(const wchar_t **)params[0], *(const wchar_t **)params[1],
                             (void **)params[2], (size_t *)params[3], (wchar_t **)params[4]);
            }
            *(int *)retval = r;
        }
        return 1;

    case AAP_SETALBUMARTDATA:
        if (retval) {
            int r = AAP_FAILURE;
            if (params && nparam >= 5) {
                r = set_art(*(const wchar_t **)params[0], *(const wchar_t **)params[1],
                            *(void **)params[2], *(size_t *)params[3],
                            *(const wchar_t **)params[4]);
            }
            *(int *)retval = r;
        }
        return 1;

    case AAP_DELETEALBUMART:
        if (retval) {
            int r = AAP_FAILURE;
            if (params && nparam >= 2) r = delete_art(*(const wchar_t **)params[0]);
            *(int *)retval = r;
        }
        return 1;

    default:
        return 0;
    }
}

static int __attribute__((thiscall)) factory_dispatch(void *self, int msg,
        void *retval, void **params, int nparam)
{
    (void)self;
    (void)nparam;

    ALOG("factory_dispatch msg=%d", msg);
    switch (msg) {
    case FACTORY_GETSERVICETYPE:
        if (retval) *(unsigned int *)retval = AAP_SERVICETYPE;
        return 1;

    case FACTORY_GETSERVICENAME:
        if (retval) *(const char **)retval = "in_opus album art provider";
        return 1;

    case FACTORY_GETGUID:
        if (retval) *(GUID *)retval = in_opus_albumart_guid;
        return 1;

    case FACTORY_GETINTERFACE:
        if (retval) *(void **)retval = (void *)&g_provider_object;
        return 1;

    case FACTORY_NONLOCKING:
    case FACTORY_RELEASE:
    case FACTORY_SERVICENOTIFY:
        if (retval) *(int *)retval = 1;
        return 1;

    case FACTORY_GETTESTSTRING:
        if (retval) *(const wchar_t **)retval = L"in_opus";
        return 1;

    case -1:                                    /* Dispatchable::ADDREF  */
    case -2:                                    /* Dispatchable::RELEASE */
        if (retval) *(size_t *)retval = 1;
        return 1;

    default:
        (void)params;
        return 0;
    }
}

/****************************************************************************
 *  Registration                                                            *
 ****************************************************************************/
static int g_registered = 0;

void albumart_register(HWND winamp)
{
    void *svc, *factory;

    if (g_registered || !winamp) return;

    svc = (void *)SendMessage(winamp, WM_WA_IPC, 0, IPC_GET_API_SERVICE);
    ALOG("albumart_register: winamp=%p api_service=%p", (void *)winamp, svc);
    if (!svc || svc == (void *)1) return;       /* Winamp older than 5.12 */

    g_svc = svc;

    factory = svc_by_guid(svc, &memmgr_guid);
    if (factory) g_memmgr = factory_get_interface(factory);

    provider_vtbl[0] = provider_dispatch;
    factory_vtbl[0] = factory_dispatch;
    g_provider_object.vtbl = provider_vtbl;
    g_factory_object.vtbl = factory_vtbl;

    ALOG("memmgr factory=%p interface=%p", factory, g_memmgr);
    {   /* never put the registration inside ALOG: it compiles away when
           ART_LOG is 0 */
        int r = svc_register(svc, &g_factory_object);
        ALOG("service_register(factory=%p) -> %d", (void *)&g_factory_object, r);
        (void)r;
    }
    g_registered = 1;
}

void albumart_unregister(void)
{
    g_registered = 0;
    g_svc = NULL;
    g_memmgr = NULL;
}