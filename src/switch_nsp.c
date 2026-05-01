/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

#if SIGIL_WITH_SWITCH

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define NCA_HEADER_SIZE 0xC00

/* Forward decls for the XCI walker (lives in switch_xci.c). */
int sigil_extract_xci(const sigil_io *io, const sigil_options *opts,
                      sigil_result *out);

static bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Scan an ASCII string-table buffer for `[0-9a-fA-F]{16}\.nca`, return
 * uppercased hex into `out_title_id` (17 bytes incl NUL) on first hit
 * whose hex prefix is "01". */
static int scan_string_table_for_nca_title(const uint8_t *table, size_t len,
                                            char out_title_id[17]) {
    if (len < 20) return SIGIL_ERR_NOT_FOUND;
    for (size_t i = 0; i + 20 <= len; i++) {
        bool ok = true;
        for (int k = 0; k < 16; k++) {
            if (!is_hex_char((char)table[i + k])) { ok = false; break; }
        }
        if (!ok) continue;
        if (table[i + 16] != '.' ||
            (table[i + 17] != 'n' && table[i + 17] != 'N') ||
            (table[i + 18] != 'c' && table[i + 18] != 'C') ||
            (table[i + 19] != 'a' && table[i + 19] != 'A')) continue;

        char tmp[17];
        for (int k = 0; k < 16; k++) {
            char c = (char)table[i + k];
            tmp[k] = (c >= 'a' && c <= 'f') ? (char)(c - 32) : c;
        }
        tmp[16] = '\0';
        if (tmp[0] == '0' && tmp[1] == '1') {
            memcpy(out_title_id, tmp, 17);
            return SIGIL_OK;
        }
    }
    return SIGIL_ERR_NOT_FOUND;
}

/* Walk a PFS0 partition starting at `partition_off`. Returns SIGIL_OK and
 * fills `out_title_id` if a title id was extracted. `header_key_or_null`
 * is the 32-byte AES-XTS key, or NULL to skip the encrypted-NCA path. */
int sigil_pfs0_extract_title_id(const sigil_io *io, uint64_t partition_off,
                                 const uint8_t *header_key_or_null,
                                 char out_title_id[17]) {
    uint8_t hdr[16];
    int rc = sigil_io_read_exact(io, partition_off, hdr, 16);
    if (rc != SIGIL_OK) return rc;
    if (memcmp(hdr, "PFS0", 4) != 0) return SIGIL_ERR_NOT_FOUND;

    uint32_t file_count        = sigil_read_le32(hdr + 4);
    uint32_t string_table_size = sigil_read_le32(hdr + 8);
    /* hdr[12..16] reserved. */

    /* Sanity caps. A real Switch package has tens of NCAs; reject anything
     * suggesting hundreds of GB of header data. */
    if (file_count > 4096 || string_table_size > 1024 * 1024) {
        return SIGIL_ERR_NOT_FOUND;
    }

    size_t entries_size = (size_t)file_count * 24;
    uint8_t *entries = (uint8_t *)malloc(entries_size);
    if (!entries) return SIGIL_ERR_OOM;
    rc = sigil_io_read_exact(io, partition_off + 16, entries, entries_size);
    if (rc != SIGIL_OK) { free(entries); return rc; }

    uint8_t *string_table = (uint8_t *)malloc(string_table_size);
    if (!string_table) { free(entries); return SIGIL_ERR_OOM; }
    rc = sigil_io_read_exact(io, partition_off + 16 + entries_size,
                              string_table, string_table_size);
    if (rc != SIGIL_OK) { free(entries); free(string_table); return rc; }

    uint64_t data_start = partition_off + 16 + entries_size + string_table_size;

    /* First try: scan the string table for content-id NCA filenames
     * (`[0-9a-fA-F]{16}\.nca`). This is the cheap unencrypted path. */
    if (scan_string_table_for_nca_title(string_table, string_table_size, out_title_id) == SIGIL_OK) {
        free(entries);
        free(string_table);
        return SIGIL_OK;
    }

    /* Second try: decrypt each NCA header and read program_id / rights_id.
     * Requires the AES-XTS header_key. */
    if (!header_key_or_null) {
        free(entries);
        free(string_table);
        return SIGIL_ERR_NEEDS_KEY;
    }

    rc = SIGIL_ERR_NOT_FOUND;
    for (uint32_t i = 0; i < file_count; i++) {
        const uint8_t *e = entries + i * 24;
        uint64_t file_offset = sigil_read_le64(e);
        uint64_t file_size   = sigil_read_le64(e + 8);
        uint32_t name_offset = sigil_read_le32(e + 16);
        if (name_offset >= string_table_size) continue;

        /* Find name end. */
        uint32_t name_end = name_offset;
        while (name_end < string_table_size && string_table[name_end] != 0) name_end++;
        size_t name_len = name_end - name_offset;
        if (name_len < 4) continue;

        /* Filter to .nca entries. */
        const char *name = (const char *)string_table + name_offset;
        if (name[name_len - 4] != '.' ||
            tolower((unsigned char)name[name_len - 3]) != 'n' ||
            tolower((unsigned char)name[name_len - 2]) != 'c' ||
            tolower((unsigned char)name[name_len - 1]) != 'a') continue;

        if (file_size < NCA_HEADER_SIZE) continue;

        uint8_t header[NCA_HEADER_SIZE];
        if (sigil_io_read_exact(io, data_start + file_offset, header,
                                 NCA_HEADER_SIZE) != SIGIL_OK) continue;

        sigil_aes_xts_decrypt_nintendo(header_key_or_null, 0, header, NCA_HEADER_SIZE);
        if (sigil_nca_extract_title_id(header, out_title_id) == SIGIL_OK) {
            rc = SIGIL_OK;
            break;
        }
    }

    free(entries);
    free(string_table);
    return rc;
}

/* Top-level Switch dispatch: routes between NSP (PFS0 at offset 0) and XCI
 * (HFS0 at offset 0xF000). */
int sigil_extract_switch(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    if (!io) return SIGIL_ERR_INVALID_ARG;

    sigil_result_init(out);
    out->platform = SIGIL_PLATFORM_SWITCH;
    out->usage = SIGIL_USAGE_FOLDER_EXACT;

    /* Probe magic bytes. */
    uint8_t magic[4];
    int rc = sigil_io_read_exact(io, 0, magic, 4);
    if (rc != SIGIL_OK) return rc;

    /* Try resolving header key from support struct. NULL is OK for the
     * unencrypted-NCA filename path (decrypted dumps and homebrew). */
    uint8_t hkey[32];
    bool have_key = (opts && opts->support
                     && sigil_resolve_header_key(opts->support, hkey) == SIGIL_OK);

    if (memcmp(magic, "PFS0", 4) == 0) {
        /* NSP: PFS0 partition is the whole file from offset 0. */
        char tid[17];
        rc = sigil_pfs0_extract_title_id(io, 0, have_key ? hkey : NULL, tid);
        if (rc != SIGIL_OK) return rc;
        memcpy(out->title_id, tid, 17);
        memcpy(out->raw_serial, tid, 17);
        out->source = SIGIL_SOURCE_BINARY;
        return SIGIL_OK;
    }

    /* Otherwise try XCI (HFS0 root at 0xF000). */
    return sigil_extract_xci(io, opts, out);
}

#endif /* SIGIL_WITH_SWITCH */
