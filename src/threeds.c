// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

#if SIGIL_WITH_3DS

#include <stdlib.h>
#include <zstd.h>

/* NCSD: NCCH partition begins at file offset 0x4000; program ID at +0x118.
 * Absolute offset 0x4118. Retail filter requires "0004" prefix. */

#define NCSD_PROGRAM_ID_ABS_OFFSET 0x4118
#define NCSD_PROGRAM_ID_LEN        8
#define ZSTD_PREFIX_NEEDED         (NCSD_PROGRAM_ID_ABS_OFFSET + NCSD_PROGRAM_ID_LEN)
#define ZSTD_CHUNK_SIZE            (64 * 1024)

static bool retail_3ds(const char *tid) {
    return tid[0] == '0' && tid[1] == '0' && tid[2] == '0' && tid[3] == '4';
}

static int read_zstd_prefix(const sigil_io *io, uint8_t *out_buf, size_t needed) {
    ZSTD_DCtx *dctx = ZSTD_createDCtx();
    if (!dctx) return SIGIL_ERR_OOM;

    uint8_t *in_buf = (uint8_t *)malloc(ZSTD_CHUNK_SIZE);
    if (!in_buf) { ZSTD_freeDCtx(dctx); return SIGIL_ERR_OOM; }

    uint64_t in_off = 0;
    size_t out_pos = 0;
    int rc = SIGIL_ERR_NOT_FOUND;

    while (out_pos < needed) {
        int got = io->read(io->ctx, in_off, in_buf, ZSTD_CHUNK_SIZE);
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
    sigil_lower_ext(filename_hint, ext);
    bool is_zstd = (strcmp(ext, "z3ds") == 0 || strcmp(ext, "zcci") == 0);

    uint8_t pid[NCSD_PROGRAM_ID_LEN];
    int rc;

    if (is_zstd) {
        uint8_t *prefix = (uint8_t *)malloc(ZSTD_PREFIX_NEEDED);
        if (!prefix) return SIGIL_ERR_OOM;
        rc = read_zstd_prefix(io, prefix, ZSTD_PREFIX_NEEDED);
        if (rc != SIGIL_OK) { free(prefix); return rc; }
        memcpy(pid, prefix + NCSD_PROGRAM_ID_ABS_OFFSET, NCSD_PROGRAM_ID_LEN);
        free(prefix);
    } else {
        rc = sigil_io_read_exact(io, NCSD_PROGRAM_ID_ABS_OFFSET, pid, NCSD_PROGRAM_ID_LEN);
        if (rc != SIGIL_OK) return rc;
    }

    char tid[17];
    sigil_hex_encode_8(pid, tid, true);

    bool allow_hb = opts && (opts->flags & SIGIL_FLAG_3DS_ALLOW_HOMEBREW);
    if (!allow_hb && !retail_3ds(tid)) return SIGIL_ERR_NOT_FOUND;

    memcpy(out->title_id,  tid, 17);
    memcpy(out->raw_serial, tid, 17);
    out->source = SIGIL_SOURCE_BINARY;
    return SIGIL_OK;
}

#endif
