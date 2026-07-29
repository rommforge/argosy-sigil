// SPDX-License-Identifier: MPL-2.0
#include "sigil.h"
#include <stdio.h>
#include <string.h>

/* NCSD program id lives at absolute offset 0x4118 (NCCH at 0x4000, id at
 * +0x118), 8 bytes. Retail ids start with 0x0004. */
#define NCSD_PROGRAM_ID_ABS_OFFSET 0x4118

typedef struct { const uint8_t *buf; size_t len; } mem_ctx;

static int mem_read(void *ctx, uint64_t off, void *buf, size_t len) {
    mem_ctx *m = (mem_ctx *)ctx;
    if (off >= m->len) return 0;
    size_t avail = m->len - (size_t)off;
    size_t n = len < avail ? len : avail;
    memcpy(buf, m->buf + off, n);
    return (int)n;
}
static int64_t mem_size(void *ctx) { return (int64_t)((mem_ctx *)ctx)->len; }

int main(void) {
    static uint8_t buf[0x4200];
    memset(buf, 0, sizeof(buf));
    /* On-disk program id is little-endian; reversed it reads "0004000000033500". */
    const uint8_t pid[8] = { 0x00, 0x35, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00 };
    memcpy(buf + NCSD_PROGRAM_ID_ABS_OFFSET, pid, sizeof(pid));

    mem_ctx ctx = { buf, sizeof(buf) };
    sigil_io io = { mem_read, mem_size, NULL, &ctx };
    sigil_result r;
    int rc = sigil_extract_from_io(&io, "game.3ds", SIGIL_PLATFORM_3DS, NULL, &r);
    if (rc != SIGIL_OK) {
        fprintf(stderr, "FAIL extract: rc=%d\n", rc);
        return 1;
    }
    if (strcmp(r.title_id, "0004000000033500") != 0) {
        fprintf(stderr, "FAIL title_id: got '%s'\n", r.title_id);
        return 1;
    }
    /* save_id must carry the on-disk split, not the flat id. */
    if (strcmp(r.save_id, "00040000/00033500") != 0) {
        fprintf(stderr, "FAIL save_id: got '%s' (want 00040000/00033500)\n", r.save_id);
        return 1;
    }
    if (r.usage != SIGIL_USAGE_FOLDER_SPLIT) {
        fprintf(stderr, "FAIL usage: got %d (want folder-split)\n", r.usage);
        return 1;
    }
    printf("ok unit_3ds (save_id=%s)\n", r.save_id);
    return 0;
}
