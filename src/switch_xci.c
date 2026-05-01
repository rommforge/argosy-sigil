/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

#if SIGIL_WITH_SWITCH

#include <stdlib.h>
#include <string.h>

#define HFS0_ROOT_OFFSET 0xF000

/* PFS0 helper exported by switch_nsp.c. */
int sigil_pfs0_extract_title_id(const sigil_io *io, uint64_t partition_off,
                                 const uint8_t *header_key_or_null,
                                 char out_title_id[17]);

/* HFS0 file entries are 64 bytes (vs PFS0's 24). Layout per entry:
 *   0x00  uint64  file offset (relative to data start)
 *   0x08  uint64  file size
 *   0x10  uint32  name offset (in string table)
 *   0x14  uint32  hashed bytes
 *   0x18  uint64  reserved
 *   0x20  uint8[32]  SHA-256 hash
 */

/* Walk an HFS0 partition starting at `part_off`. Looks for an entry named
 * `target_name` (e.g. "secure"), and on hit fills *out_off + *out_size with
 * its position in the file. */
static int hfs0_find_partition(const sigil_io *io, uint64_t part_off,
                                const char *target_name,
                                uint64_t *out_off, uint64_t *out_size) {
    uint8_t hdr[16];
    int rc = sigil_io_read_exact(io, part_off, hdr, 16);
    if (rc != SIGIL_OK) return rc;
    if (memcmp(hdr, "HFS0", 4) != 0) return SIGIL_ERR_NOT_FOUND;

    uint32_t file_count        = sigil_read_le32(hdr + 4);
    uint32_t string_table_size = sigil_read_le32(hdr + 8);
    if (file_count > 4096 || string_table_size > 1024 * 1024) {
        return SIGIL_ERR_NOT_FOUND;
    }

    size_t entries_size = (size_t)file_count * 64;
    uint8_t *entries = (uint8_t *)malloc(entries_size);
    if (!entries) return SIGIL_ERR_OOM;
    rc = sigil_io_read_exact(io, part_off + 16, entries, entries_size);
    if (rc != SIGIL_OK) { free(entries); return rc; }

    uint8_t *string_table = (uint8_t *)malloc(string_table_size);
    if (!string_table) { free(entries); return SIGIL_ERR_OOM; }
    rc = sigil_io_read_exact(io, part_off + 16 + entries_size,
                              string_table, string_table_size);
    if (rc != SIGIL_OK) { free(entries); free(string_table); return rc; }

    uint64_t data_start = part_off + 16 + entries_size + string_table_size;
    size_t target_len = strlen(target_name);

    rc = SIGIL_ERR_NOT_FOUND;
    for (uint32_t i = 0; i < file_count; i++) {
        const uint8_t *e = entries + i * 64;
        uint64_t fo = sigil_read_le64(e);
        uint64_t fs = sigil_read_le64(e + 8);
        uint32_t name_off = sigil_read_le32(e + 16);
        if (name_off >= string_table_size) continue;

        size_t name_len = 0;
        while (name_off + name_len < string_table_size
               && string_table[name_off + name_len] != 0) name_len++;
        if (name_len == target_len
            && memcmp(string_table + name_off, target_name, target_len) == 0) {
            *out_off = data_start + fo;
            *out_size = fs;
            rc = SIGIL_OK;
            break;
        }
    }

    free(entries);
    free(string_table);
    return rc;
}

/* HFS0 partition's `secure` sub-partition is itself a PFS0-shaped (HFS0)
 * partition containing the .nca files we need. The NCA structure is the
 * same as in NSP, so we walk it with the PFS0 helper. */
static int hfs0_extract_title_from_partition(const sigil_io *io,
                                              uint64_t part_off,
                                              const uint8_t *header_key_or_null,
                                              char out_title_id[17]) {
    /* Read the inner partition's header — it's HFS0 too, but the NCA file
     * layout is what matters. We treat it like PFS0 with HFS0's 64-byte
     * entries; rather than duplicating the parser, walk it with HFS0
     * entry layout here. */
    uint8_t hdr[16];
    int rc = sigil_io_read_exact(io, part_off, hdr, 16);
    if (rc != SIGIL_OK) return rc;
    if (memcmp(hdr, "HFS0", 4) != 0) return SIGIL_ERR_NOT_FOUND;

    uint32_t file_count        = sigil_read_le32(hdr + 4);
    uint32_t string_table_size = sigil_read_le32(hdr + 8);
    if (file_count > 4096 || string_table_size > 1024 * 1024) {
        return SIGIL_ERR_NOT_FOUND;
    }

    size_t entries_size = (size_t)file_count * 64;
    uint8_t *entries = (uint8_t *)malloc(entries_size);
    if (!entries) return SIGIL_ERR_OOM;
    rc = sigil_io_read_exact(io, part_off + 16, entries, entries_size);
    if (rc != SIGIL_OK) { free(entries); return rc; }

    uint8_t *string_table = (uint8_t *)malloc(string_table_size);
    if (!string_table) { free(entries); return SIGIL_ERR_OOM; }
    rc = sigil_io_read_exact(io, part_off + 16 + entries_size,
                              string_table, string_table_size);
    if (rc != SIGIL_OK) { free(entries); free(string_table); return rc; }

    uint64_t data_start = part_off + 16 + entries_size + string_table_size;

    rc = SIGIL_ERR_NOT_FOUND;
    if (!header_key_or_null) {
        free(entries); free(string_table);
        return SIGIL_ERR_NEEDS_KEY;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        const uint8_t *e = entries + i * 64;
        uint64_t fo = sigil_read_le64(e);
        uint64_t fs = sigil_read_le64(e + 8);
        uint32_t name_off = sigil_read_le32(e + 16);
        if (name_off >= string_table_size) continue;

        size_t name_len = 0;
        while (name_off + name_len < string_table_size
               && string_table[name_off + name_len] != 0) name_len++;
        if (name_len < 4) continue;
        const uint8_t *name = string_table + name_off;
        if (name[name_len - 4] != '.') continue;
        char a = (char)name[name_len - 3];
        char b = (char)name[name_len - 2];
        char c = (char)name[name_len - 1];
        if ((a != 'n' && a != 'N') || (b != 'c' && b != 'C') || (c != 'a' && c != 'A')) continue;
        if (fs < 0xC00) continue;

        uint8_t header[0xC00];
        if (sigil_io_read_exact(io, data_start + fo, header, 0xC00) != SIGIL_OK) continue;
        sigil_aes_xts_decrypt_nintendo(header_key_or_null, 0, header, 0xC00);
        if (sigil_nca_extract_title_id(header, out_title_id) == SIGIL_OK) {
            rc = SIGIL_OK;
            break;
        }
    }

    free(entries);
    free(string_table);
    return rc;
}

int sigil_extract_xci(const sigil_io *io, const sigil_options *opts,
                      sigil_result *out) {
    /* XCI carries a CGA gamecard header at offset 0; the root HFS0 sits at
     * 0xF000. Validate the root, find "secure", recurse. */
    uint64_t secure_off, secure_size;
    int rc = hfs0_find_partition(io, HFS0_ROOT_OFFSET, "secure",
                                  &secure_off, &secure_size);
    if (rc != SIGIL_OK) return rc;
    (void)secure_size;

    uint8_t hkey[32];
    bool have_key = (opts && opts->support
                     && sigil_resolve_header_key(opts->support, hkey) == SIGIL_OK);

    char tid[17];
    rc = hfs0_extract_title_from_partition(io, secure_off,
                                            have_key ? hkey : NULL, tid);
    if (rc != SIGIL_OK) return rc;

    memcpy(out->title_id, tid, 17);
    memcpy(out->raw_serial, tid, 17);
    out->source = SIGIL_SOURCE_BINARY;
    return SIGIL_OK;
}

#endif /* SIGIL_WITH_SWITCH */
