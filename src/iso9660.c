/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <string.h>

int sigil_iso_read_sector(const sigil_io *io, uint32_t lba, uint8_t out[SIGIL_ISO_SECTOR_SIZE]) {
    return sigil_io_read_exact(io, (uint64_t)lba * SIGIL_ISO_SECTOR_SIZE, out, SIGIL_ISO_SECTOR_SIZE);
}

int sigil_iso_read_pvd_root(const sigil_io *io, uint32_t *root_lba, uint32_t *root_len) {
    uint8_t pvd[SIGIL_ISO_SECTOR_SIZE];
    int rc = sigil_iso_read_sector(io, 16, pvd);
    if (rc != SIGIL_OK) return rc;

    /* PVD signature: type byte 0x01 followed by "CD001" */
    if (pvd[0] != 0x01 || memcmp(pvd + 1, "CD001", 5) != 0) {
        return SIGIL_ERR_NOT_FOUND;
    }

    /* Root directory record begins at offset 156 in the PVD.
     * Within that 34-byte record:
     *   +2  : 8-byte LBA (LE then BE; we read LE half)
     *   +10 : 8-byte length (LE then BE; we read LE half) */
    *root_lba = sigil_read_le32(pvd + 156 + 2);
    *root_len = sigil_read_le32(pvd + 156 + 10);
    return SIGIL_OK;
}

static int casecmp_n(const char *a, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
    }
    return 0;
}

int sigil_iso_find_file(const sigil_io *io,
                        uint32_t dir_lba, uint32_t dir_len,
                        const char *target_name,
                        sigil_iso_file_loc *out) {
    if (!target_name || !out) return SIGIL_ERR_INVALID_ARG;
    size_t target_len = strlen(target_name);
    /* Cap directory traversal at 4 sectors — matches argosy's Iso9660Utils.kt
     * coverage and keeps memory bounded for malformed images. */
    uint32_t sectors = (dir_len + SIGIL_ISO_SECTOR_SIZE - 1) / SIGIL_ISO_SECTOR_SIZE;
    if (sectors > 4) sectors = 4;

    uint8_t buf[SIGIL_ISO_SECTOR_SIZE];

    for (uint32_t s = 0; s < sectors; s++) {
        int rc = sigil_iso_read_sector(io, dir_lba + s, buf);
        if (rc != SIGIL_OK) continue;

        size_t pos = 0;
        while (pos < SIGIL_ISO_SECTOR_SIZE) {
            uint8_t rec_len = buf[pos];
            if (rec_len == 0) {
                /* Padding to next logical sector boundary within this read. */
                size_t next = ((pos / SIGIL_ISO_SECTOR_SIZE) + 1) * SIGIL_ISO_SECTOR_SIZE;
                pos = next;
                continue;
            }
            if (pos + rec_len > SIGIL_ISO_SECTOR_SIZE) break;

            uint8_t name_len = buf[pos + 32];
            if (name_len > 0 && pos + 33 + name_len <= SIGIL_ISO_SECTOR_SIZE) {
                const uint8_t *name = buf + pos + 33;
                /* Match either NAME or NAME;1 form, case-insensitive. */
                bool matches = false;
                if (name_len == target_len && casecmp_n(target_name, name, target_len) == 0) {
                    matches = true;
                } else if (name_len == target_len + 2
                           && casecmp_n(target_name, name, target_len) == 0
                           && name[target_len] == ';' && name[target_len + 1] == '1') {
                    matches = true;
                }
                if (matches) {
                    out->lba = sigil_read_le32(buf + pos + 2);
                    out->length = sigil_read_le32(buf + pos + 10);
                    return SIGIL_OK;
                }
            }
            pos += rec_len;
        }
    }
    return SIGIL_ERR_NOT_FOUND;
}
