/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a 64-char hex string into 32 bytes. */
static int decode_hex32(const char *hex, size_t len, uint8_t out[32]) {
    if (len != 64) return SIGIL_ERR_INVALID_ARG;
    for (int i = 0; i < 32; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return SIGIL_ERR_INVALID_ARG;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return SIGIL_OK;
}

/* Find a `key_name = <hex>` line in prod.keys text. Returns hex span on
 * success; both pointers null on miss. */
static int find_key_hex(const char *text, size_t text_len, const char *key_name,
                        const char **hex_start, size_t *hex_len) {
    size_t kn_len = strlen(key_name);
    const char *cur = text;
    const char *end = text + text_len;

    while (cur < end) {
        /* Find start of next line. */
        const char *line_end = memchr(cur, '\n', (size_t)(end - cur));
        if (!line_end) line_end = end;

        /* Skip leading whitespace. */
        const char *p = cur;
        while (p < line_end && isspace((unsigned char)*p)) p++;

        if (p + kn_len <= line_end && memcmp(p, key_name, kn_len) == 0) {
            const char *q = p + kn_len;
            /* Must be followed by space, tab, or `=`. */
            if (q < line_end && (*q == ' ' || *q == '\t' || *q == '=')) {
                /* Skip to '=' and past it. */
                while (q < line_end && *q != '=') q++;
                if (q < line_end && *q == '=') {
                    q++;
                    while (q < line_end && isspace((unsigned char)*q)) q++;
                    /* Trim trailing whitespace. */
                    const char *e = line_end;
                    while (e > q && isspace((unsigned char)*(e - 1))) e--;
                    *hex_start = q;
                    *hex_len = (size_t)(e - q);
                    return SIGIL_OK;
                }
            }
        }

        cur = (line_end < end) ? line_end + 1 : end;
    }
    return SIGIL_ERR_NOT_FOUND;
}

int sigil_load_header_key_from_prod_keys(const char *path, uint8_t out[32]) {
    if (!path || !out) return SIGIL_ERR_INVALID_ARG;

    FILE *fp = fopen(path, "rb");
    if (!fp) return SIGIL_ERR_IO;
    if (fseeko(fp, 0, SEEK_END) != 0) { fclose(fp); return SIGIL_ERR_IO; }
    off_t sz = ftello(fp);
    if (sz < 0 || sz > 1024 * 1024) { fclose(fp); return SIGIL_ERR_IO; }
    rewind(fp);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return SIGIL_ERR_OOM; }
    size_t got = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[got] = '\0';

    const char *hex; size_t hex_len;
    int rc = find_key_hex(buf, got, "header_key", &hex, &hex_len);
    if (rc != SIGIL_OK) { free(buf); return rc; }

    rc = decode_hex32(hex, hex_len, out);
    free(buf);
    return rc;
}

/* Internal: extract header_key from an in-memory blob. Used when the
 * caller passes prod.keys content via sigil_support.switch_prod_keys_text. */
int sigil_decode_header_key_from_text(const char *text, size_t text_len,
                                       uint8_t out[32]) {
    if (!text || !out) return SIGIL_ERR_INVALID_ARG;
    const char *hex; size_t hex_len;
    int rc = find_key_hex(text, text_len, "header_key", &hex, &hex_len);
    if (rc != SIGIL_OK) return rc;
    return decode_hex32(hex, hex_len, out);
}
