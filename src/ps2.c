// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"
#include <stdlib.h>

#define PS2_SAVE_PREFIX     "BA"
#define PS2_SAVE_PREFIX_LEN 2
#define PS2_REGION_LEN      4
#define PS2_SERIAL_DIGITS   5
#define PS2_SAVE_CORE_LEN   (PS2_SAVE_PREFIX_LEN + PS2_REGION_LEN + PS2_SERIAL_DIGITS)
#define PS2_ELF_SCAN_CHUNK  (64 * 1024)

static bool ps2_alphanumeric(uint8_t c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z');
}

static bool ps2_match_core(const uint8_t *p, size_t avail,
                           const char *region, const char *digits) {
    if (avail < PS2_SAVE_CORE_LEN) return false;
    if (p[0] != 'B' || p[1] < 'A' || p[1] > 'Z') return false;
    for (int i = 0; i < PS2_REGION_LEN; i++) {
        if (p[PS2_SAVE_PREFIX_LEN + i] != (uint8_t)region[i]) return false;
    }
    size_t off = PS2_SAVE_PREFIX_LEN + PS2_REGION_LEN;
    if (p[off] == '-') {
        off++;
        if (avail < off + PS2_SERIAL_DIGITS) return false;
    }
    for (int i = 0; i < PS2_SERIAL_DIGITS; i++) {
        if (p[off + i] != (uint8_t)digits[i]) return false;
    }
    return true;
}

static size_t ps2_match_length(const uint8_t *p, size_t avail,
                               const char *region, const char *digits) {
    if (!ps2_match_core(p, avail, region, digits)) return 0;
    size_t off = PS2_SAVE_PREFIX_LEN + PS2_REGION_LEN;
    if (p[off] == '-') off++;
    off += PS2_SERIAL_DIGITS;
    if (off < avail && ps2_alphanumeric(p[off])) off++;
    return off;
}

static int ps2_scan_elf_for_save_id(const sigil_io *io,
                                    const sigil_iso_file_loc *elf,
                                    const char title_id[32],
                                    char out_save_id[32]) {
    const char *region = title_id;
    const char *digits = title_id + PS2_REGION_LEN + 1;

    uint8_t *buf = (uint8_t *)malloc(PS2_ELF_SCAN_CHUNK);
    if (!buf) return SIGIL_ERR_OOM;

    const size_t carry = PS2_SAVE_CORE_LEN + 2;
    uint64_t base = (uint64_t)elf->lba * SIGIL_ISO_SECTOR_SIZE;
    uint64_t pos = 0;
    size_t prev = 0;

    while (pos < elf->length) {
        size_t want = PS2_ELF_SCAN_CHUNK - prev;
        size_t remain = (size_t)(elf->length - pos);
        if (want > remain) want = remain;

        int got = io->read(io->ctx, base + pos, buf + prev, want);
        if (got <= 0) break;
        size_t avail = prev + (size_t)got;

        size_t end = avail;
        if ((uint64_t)got == want && (pos + (uint64_t)got) < elf->length && avail > carry) {
            end = avail - carry;
        }

        for (size_t i = 0; i + PS2_SAVE_CORE_LEN <= avail && i < end; i++) {
            size_t hit = ps2_match_length(buf + i, avail - i, region, digits);
            if (hit && hit < sizeof(((sigil_result *)0)->save_id)) {
                memcpy(out_save_id, buf + i, hit);
                out_save_id[hit] = '\0';
                free(buf);
                return SIGIL_OK;
            }
        }

        if (end < avail) {
            size_t tail = avail - end;
            memmove(buf, buf + end, tail);
            prev = tail;
        } else {
            prev = 0;
        }
        pos += (uint64_t)got;
    }

    free(buf);
    return SIGIL_ERR_NOT_FOUND;
}

static const char *ps2_region_prefix(const char title_id[32]) {
    switch (title_id[2]) {
    case 'E': return "BE";
    case 'P': case 'J': case 'K': return "BI";
    default:  return PS2_SAVE_PREFIX;
    }
}

static void ps2_default_save_id(const char title_id[32], char out_save_id[32]) {
    size_t len = strlen(title_id);
    if (len + PS2_SAVE_PREFIX_LEN >= 32) {
        out_save_id[0] = '\0';
        return;
    }
    memcpy(out_save_id, ps2_region_prefix(title_id), PS2_SAVE_PREFIX_LEN);
    memcpy(out_save_id + PS2_SAVE_PREFIX_LEN, title_id, len);
    out_save_id[PS2_SAVE_PREFIX_LEN + len] = '\0';
}

int sigil_extract_ps2(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;

    sigil_result_init(out);
    out->platform = SIGIL_PLATFORM_PS2;
    out->usage = SIGIL_USAGE_FOLDER_EXACT;

    uint32_t root_lba, root_len;
    int rc = sigil_iso_read_pvd_root(io, &root_lba, &root_len);
    if (rc != SIGIL_OK) return rc;

    sigil_iso_file_loc cnf;
    rc = sigil_iso_find_file(io, root_lba, root_len, "SYSTEM.CNF", &cnf);
    if (rc != SIGIL_OK) return rc;

    uint8_t buf[SIGIL_ISO_SECTOR_SIZE];
    rc = sigil_iso_read_sector(io, cnf.lba, buf);
    if (rc != SIGIL_OK) return rc;

    size_t scan_len = cnf.length < SIGIL_ISO_SECTOR_SIZE ? cnf.length : SIGIL_ISO_SECTOR_SIZE;
    rc = sigil_cnf_parse_boot(buf, scan_len, "BOOT2", out->raw_serial, out->title_id);
    if (rc != SIGIL_OK) return rc;

    out->source = SIGIL_SOURCE_BINARY;

    sigil_iso_file_loc elf;
    if (sigil_iso_find_file(io, root_lba, root_len, out->raw_serial, &elf) == SIGIL_OK) {
        if (ps2_scan_elf_for_save_id(io, &elf, out->title_id, out->save_id) == SIGIL_OK) {
            return SIGIL_OK;
        }
    }

    ps2_default_save_id(out->title_id, out->save_id);
    return SIGIL_OK;
}
