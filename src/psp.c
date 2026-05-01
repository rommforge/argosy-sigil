/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <string.h>

/* PSP UMD_DATA.BIN encodes the disc id followed by '|' and other fields,
 * e.g. "ULUS-10064|0123456789ABCDEF|...". The dashed form is what the disc
 * carries; the dashless form (`ULUS10064`) is what PSP's SAVEDATA folder
 * convention uses. */

static int parse_disc_id(const uint8_t *buf, size_t len,
                         char raw[32], char canonical[32]) {
    /* Bound the scan: the candidate is everything before the first '|'. */
    const uint8_t *pipe = (const uint8_t *)memchr(buf, '|', len);
    if (!pipe) return SIGIL_ERR_NOT_FOUND;
    size_t cand_len = (size_t)(pipe - buf);

    /* Trim trailing whitespace. */
    while (cand_len > 0 && isspace((unsigned char)buf[cand_len - 1])) cand_len--;
    /* Skip leading whitespace. */
    size_t start = 0;
    while (start < cand_len && isspace((unsigned char)buf[start])) start++;
    cand_len -= start;
    if (cand_len > 31) return SIGIL_ERR_NOT_FOUND;

    /* Validate `^([A-Z]{4})-?(\d{5})$` (the regex used in Iso9660Utils.kt). */
    char tmp[32];
    memcpy(tmp, buf + start, cand_len);
    tmp[cand_len] = '\0';

    /* Check shape: 9 or 10 chars. */
    if (cand_len != 9 && cand_len != 10) return SIGIL_ERR_NOT_FOUND;
    for (int i = 0; i < 4; i++) {
        if (tmp[i] < 'A' || tmp[i] > 'Z') return SIGIL_ERR_NOT_FOUND;
    }
    size_t digits_off = 4;
    if (cand_len == 10) {
        if (tmp[4] != '-') return SIGIL_ERR_NOT_FOUND;
        digits_off = 5;
    }
    for (int i = 0; i < 5; i++) {
        char c = tmp[digits_off + i];
        if (c < '0' || c > '9') return SIGIL_ERR_NOT_FOUND;
    }

    /* raw = preserve original (always with dash, since UMD_DATA.BIN spec uses
     * the dashed form; if the disc happened to omit it we still emit the
     * dashed form for raw to match argosy's `TitleIdResult` documentation
     * convention of "as it appears in the binary"). */
    if (cand_len == 9) {
        /* Insert dash for raw. */
        memcpy(raw, tmp, 4);
        raw[4] = '-';
        memcpy(raw + 5, tmp + 4, 5);
        raw[10] = '\0';
    } else {
        memcpy(raw, tmp, 10);
        raw[10] = '\0';
    }

    /* canonical = always dashless 9-char. */
    memcpy(canonical, tmp, 4);
    memcpy(canonical + 4, tmp + digits_off, 5);
    canonical[9] = '\0';
    return SIGIL_OK;
}

int sigil_extract_psp(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;

    sigil_result_init(out);
    out->platform = SIGIL_PLATFORM_PSP;
    out->usage = SIGIL_USAGE_FOLDER_PREFIX;

    uint32_t root_lba, root_len;
    int rc = sigil_iso_read_pvd_root(io, &root_lba, &root_len);
    if (rc != SIGIL_OK) return rc;

    sigil_iso_file_loc umd;
    rc = sigil_iso_find_file(io, root_lba, root_len, "UMD_DATA.BIN", &umd);
    if (rc != SIGIL_OK) return rc;

    uint8_t buf[SIGIL_ISO_SECTOR_SIZE];
    rc = sigil_iso_read_sector(io, umd.lba, buf);
    if (rc != SIGIL_OK) return rc;

    size_t scan_len = umd.length < SIGIL_ISO_SECTOR_SIZE ? umd.length : SIGIL_ISO_SECTOR_SIZE;
    if (scan_len < 10) return SIGIL_ERR_NOT_FOUND;

    rc = parse_disc_id(buf, scan_len, out->raw_serial, out->title_id);
    if (rc != SIGIL_OK) return rc;

    out->source = SIGIL_SOURCE_BINARY;
    return SIGIL_OK;
}
