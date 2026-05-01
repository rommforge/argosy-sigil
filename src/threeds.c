/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

#if SIGIL_WITH_3DS

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zstd.h>

/* 3DS NCSD format: NCCH partition begins at file offset 0x4000; program ID
 * sits at +0x118 (8 bytes little-endian). Absolute offset = 0x4118. The
 * program ID is the canonical 16-hex title id once byte-reversed and
 * upper-cased. Retail filter: must start with `0004` (commit 4f34ace9). */

#define NCSD_PROGRAM_ID_ABS_OFFSET 0x4118
#define NCSD_PROGRAM_ID_LEN        8
#define ZSTD_PREFIX_NEEDED         (NCSD_PROGRAM_ID_ABS_OFFSET + NCSD_PROGRAM_ID_LEN)

static void hex16_upper_reversed(const uint8_t *src8, char out[17]) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        uint8_t b = src8[7 - i];
        out[i * 2]     = hex[(b >> 4) & 0xF];
        out[i * 2 + 1] = hex[b & 0xF];
    }
    out[16] = '\0';
}

static bool retail_3ds(const char *tid) {
    return tid[0] == '0' && tid[1] == '0' && tid[2] == '0' && tid[3] == '4';
}

/* Lower-case extension extractor — same shape as in sigil.c but private here
 * to avoid pulling that dispatcher's helpers into a per-platform unit. */
static void lower_ext(const char *name, char out[16]) {
    out[0] = '\0';
    if (!name) return;
    const char *dot = strrchr(name, '.');
    if (!dot) return;
    dot++;
    size_t i = 0;
    while (dot[i] && i < 15) {
        out[i] = (char)tolower((unsigned char)dot[i]);
        i++;
    }
    out[i] = '\0';
}

/* Decompress at least `needed` bytes from the zstd stream backed by `io`.
 * Writes the prefix into `out_buf` (size = `needed`). Returns SIGIL_OK on
 * success. */
static int read_zstd_prefix(const sigil_io *io, uint8_t *out_buf, size_t needed) {
    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    if (!dctx) return SIGIL_ERR_OOM;

    /* Stream-decompress in 64 KB chunks until we accumulate `needed` bytes. */
    const size_t IN_CAP = 64 * 1024;
    uint8_t *in_buf = (uint8_t *)malloc(IN_CAP);
    if (!in_buf) { ZSTD_freeDCtx(dctx); return SIGIL_ERR_OOM; }

    uint64_t in_off = 0;
    size_t out_pos = 0;
    int rc = SIGIL_ERR_NOT_FOUND;

    while (out_pos < needed) {
        int got = io->read(io->ctx, in_off, in_buf, IN_CAP);
        if (got <= 0) break;
        in_off += (uint64_t)got;

        ZSTD_inBuffer  in  = { in_buf, (size_t)got, 0 };
        ZSTD_outBuffer out = { out_buf, needed, out_pos };

        while (in.pos < in.size && out.pos < needed) {
            size_t r = ZSTD_decompressStream(dctx, &out, &in);
            if (ZSTD_isError(r)) goto done;
        }
        out_pos = out.pos;
    }
    if (out_pos >= needed) rc = SIGIL_OK;

done:
    ZSTD_freeDCtx(dctx);
    free(in_buf);
    return rc;
}

int sigil_extract_3ds(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out) {
    if (!io) return SIGIL_ERR_INVALID_ARG;

    sigil_result_init(out);
    out->platform = SIGIL_PLATFORM_3DS;
    out->usage = SIGIL_USAGE_FOLDER_EXACT;

    char ext[16];
    lower_ext(filename_hint, ext);
    bool is_zstd = (strcmp(ext, "z3ds") == 0 || strcmp(ext, "zcci") == 0);

    uint8_t pid[NCSD_PROGRAM_ID_LEN];
    int rc;

    if (is_zstd) {
        /* Decompress just the first 0x4118+8 bytes from the zstd stream and
         * read the program id from the decompressed prefix. */
        uint8_t *prefix = (uint8_t *)malloc(ZSTD_PREFIX_NEEDED);
        if (!prefix) return SIGIL_ERR_OOM;
        rc = read_zstd_prefix(io, prefix, ZSTD_PREFIX_NEEDED);
        if (rc != SIGIL_OK) { free(prefix); return rc; }
        memcpy(pid, prefix + NCSD_PROGRAM_ID_ABS_OFFSET, NCSD_PROGRAM_ID_LEN);
        free(prefix);
    } else {
        /* Raw .3ds / .cci path — direct seek + read. */
        rc = sigil_io_read_exact(io, NCSD_PROGRAM_ID_ABS_OFFSET, pid, NCSD_PROGRAM_ID_LEN);
        if (rc != SIGIL_OK) return rc;
    }

    char tid[17];
    hex16_upper_reversed(pid, tid);

    bool allow_hb = opts && (opts->flags & SIGIL_FLAG_3DS_ALLOW_HOMEBREW);
    if (!allow_hb && !retail_3ds(tid)) return SIGIL_ERR_NOT_FOUND;

    memcpy(out->title_id,  tid, 17);
    memcpy(out->raw_serial, tid, 17);
    out->source = SIGIL_SOURCE_BINARY;
    return SIGIL_OK;
}

#endif /* SIGIL_WITH_3DS */
