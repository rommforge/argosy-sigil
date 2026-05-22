// SPDX-License-Identifier: MPL-2.0
#include "sigil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XEX_HEADER_EXEC_INFO 0x00040006u

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

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
    /* Build a minimal XEX2 with one optional header pointing at exec info. */
    uint8_t buf[1024];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "XEX2", 4);
    write_be32(buf + 0x14, 1);                /* one optional header */
    write_be32(buf + 0x18, XEX_HEADER_EXEC_INFO);
    write_be32(buf + 0x1C, 0x100);            /* exec_info offset */
    /* Title ID at 0x100 + 0x0C = 0x10C, four bytes */
    buf[0x10C] = 0x41; buf[0x10D] = 0x4D;
    buf[0x10E] = 0x07; buf[0x10F] = 0xD1;     /* "AM\x07\xD1" -> "41 4D 07 D1" */

    mem_ctx ctx = { buf, sizeof(buf) };
    sigil_io io = { mem_read, mem_size, NULL, &ctx };
    sigil_result r;
    int rc = sigil_extract_from_io(&io, "default.xex", SIGIL_PLATFORM_XBOX360, NULL, &r);
    if (rc != SIGIL_OK) {
        fprintf(stderr, "FAIL extract: rc=%d\n", rc);
        return 1;
    }
    if (strcmp(r.title_id, "414D07D1") != 0) {
        fprintf(stderr, "FAIL title_id: got '%s'\n", r.title_id);
        return 1;
    }
    if (!r.experimental) {
        fprintf(stderr, "FAIL: expected experimental=1\n");
        return 1;
    }
    printf("ok unit_xex (title_id=%s)\n", r.title_id);
    return 0;
}
