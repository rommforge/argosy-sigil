// SPDX-License-Identifier: MPL-2.0
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SIGIL_OK         0
#define SIGIL_ERR_NOT_FOUND -5

extern int sigil_sfo_get_string(const uint8_t *data, size_t len,
                                 const char *key,
                                 char *out, size_t out_cap);

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Build a minimal valid PARAM.SFO with two entries: TITLE_ID and TITLE.
 * Layout: header(20) | index_table(N*16) | key_table | data_table. */
static size_t build_sfo(uint8_t *buf, size_t cap,
                        const char *tid, const char *title) {
    if (cap < 4096) return 0;
    size_t header     = 20;
    size_t key_off1   = 0;
    size_t key_off2   = strlen("TITLE_ID") + 1;
    size_t key_table_size = key_off2 + strlen("TITLE") + 1;
    while (key_table_size % 4 != 0) key_table_size++;

    size_t index_table_size = 2 * 16;
    size_t key_table_start  = header + index_table_size;
    size_t data_table_start = key_table_start + key_table_size;

    size_t tid_len   = strlen(tid) + 1;
    size_t title_len = strlen(title) + 1;
    size_t total = data_table_start + tid_len + title_len;
    if (total > cap) return 0;

    memset(buf, 0, total);
    /* Header */
    buf[0] = 0x00; buf[1] = 'P'; buf[2] = 'S'; buf[3] = 'F';
    write_le32(buf + 4, 0x00000101);
    write_le32(buf + 8,  (uint32_t)key_table_start);
    write_le32(buf + 12, (uint32_t)data_table_start);
    write_le32(buf + 16, 2);

    /* Index entry: TITLE_ID -> first slot */
    uint8_t *e = buf + header;
    e[0] = (uint8_t)key_off1; e[1] = (uint8_t)(key_off1 >> 8);
    e[2] = 0x04; e[3] = 0x02;            /* utf8 string format */
    write_le32(e + 4,  (uint32_t)tid_len);
    write_le32(e + 8,  (uint32_t)tid_len);
    write_le32(e + 12, 0);

    /* Index entry: TITLE -> second slot */
    e = buf + header + 16;
    e[0] = (uint8_t)key_off2; e[1] = (uint8_t)(key_off2 >> 8);
    e[2] = 0x04; e[3] = 0x02;
    write_le32(e + 4,  (uint32_t)title_len);
    write_le32(e + 8,  (uint32_t)title_len);
    write_le32(e + 12, (uint32_t)tid_len);

    /* Key table */
    memcpy(buf + key_table_start + key_off1, "TITLE_ID", strlen("TITLE_ID") + 1);
    memcpy(buf + key_table_start + key_off2, "TITLE",    strlen("TITLE")    + 1);

    /* Data table */
    memcpy(buf + data_table_start,            tid,   tid_len);
    memcpy(buf + data_table_start + tid_len,  title, title_len);

    return total;
}

int main(void) {
    uint8_t blob[4096];
    size_t n = build_sfo(blob, sizeof(blob), "BLUS31426", "Test Game");
    if (n == 0) { fprintf(stderr, "FAIL build_sfo\n"); return 1; }

    char out[64];
    int rc = sigil_sfo_get_string(blob, n, "TITLE_ID", out, sizeof(out));
    if (rc != SIGIL_OK) { fprintf(stderr, "FAIL get TITLE_ID: rc=%d\n", rc); return 1; }
    if (strcmp(out, "BLUS31426") != 0) {
        fprintf(stderr, "FAIL TITLE_ID value: got '%s'\n", out);
        return 1;
    }

    rc = sigil_sfo_get_string(blob, n, "TITLE", out, sizeof(out));
    if (rc != SIGIL_OK || strcmp(out, "Test Game") != 0) {
        fprintf(stderr, "FAIL get TITLE: rc=%d, got '%s'\n", rc, out);
        return 1;
    }

    rc = sigil_sfo_get_string(blob, n, "MISSING", out, sizeof(out));
    if (rc != SIGIL_ERR_NOT_FOUND) {
        fprintf(stderr, "FAIL missing key should return NOT_FOUND, got rc=%d\n", rc);
        return 1;
    }

    /* Bad magic -> NOT_FOUND */
    blob[1] = 'X';
    rc = sigil_sfo_get_string(blob, n, "TITLE_ID", out, sizeof(out));
    if (rc != SIGIL_ERR_NOT_FOUND) {
        fprintf(stderr, "FAIL bad magic should return NOT_FOUND, got rc=%d\n", rc);
        return 1;
    }

    printf("ok unit_sfo\n");
    return 0;
}
