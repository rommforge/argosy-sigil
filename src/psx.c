/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

int sigil_extract_psx(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;

    sigil_result_init(out);
    out->platform = SIGIL_PLATFORM_PSX;
    out->usage = SIGIL_USAGE_FILE_PREFIX;

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
    rc = sigil_cnf_parse_boot(buf, scan_len, "BOOT", out->raw_serial, out->title_id);
    if (rc != SIGIL_OK) return rc;

    out->source = SIGIL_SOURCE_BINARY;
    return SIGIL_OK;
}
