// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

#if SIGIL_WITH_SWITCH

#include <stdlib.h>

#define HFS0_ROOT_OFFSET 0xF000
#define HFS0_ENTRY_SIZE  64

#define MAX_SWITCH_FILE_COUNT  4096
#define MAX_SWITCH_STRING_TABLE (1024 * 1024)

static int hfs0_find_partition(const sigil_io *io, uint64_t part_off,
                                const char *target_name,
                                uint64_t *out_off, uint64_t *out_size) {
    uint8_t hdr[16];
    int rc = sigil_io_read_exact(io, part_off, hdr, 16);
    if (rc != SIGIL_OK) return rc;
    if (memcmp(hdr, "HFS0", 4) != 0) return SIGIL_ERR_NOT_FOUND;

    uint32_t file_count        = sigil_read_le32(hdr + 4);
    uint32_t string_table_size = sigil_read_le32(hdr + 8);
    if (file_count > MAX_SWITCH_FILE_COUNT || string_table_size > MAX_SWITCH_STRING_TABLE) {
        return SIGIL_ERR_NOT_FOUND;
    }

    size_t entries_size = (size_t)file_count * HFS0_ENTRY_SIZE;
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
        const uint8_t *e = entries + i * HFS0_ENTRY_SIZE;
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

static int hfs0_extract_title_from_partition(const sigil_io *io,
                                              uint64_t part_off,
                                              const uint8_t *header_key_or_null,
                                              char out_title_id[17]) {
    uint8_t hdr[16];
    int rc = sigil_io_read_exact(io, part_off, hdr, 16);
    if (rc != SIGIL_OK) return rc;
    if (memcmp(hdr, "HFS0", 4) != 0) return SIGIL_ERR_NOT_FOUND;

    uint32_t file_count        = sigil_read_le32(hdr + 4);
    uint32_t string_table_size = sigil_read_le32(hdr + 8);
    if (file_count > MAX_SWITCH_FILE_COUNT || string_table_size > MAX_SWITCH_STRING_TABLE) {
        return SIGIL_ERR_NOT_FOUND;
    }

    size_t entries_size = (size_t)file_count * HFS0_ENTRY_SIZE;
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

    bool needs_key = false;
    rc = SIGIL_ERR_NOT_FOUND;
    for (uint32_t i = 0; i < file_count; i++) {
        const uint8_t *e = entries + i * HFS0_ENTRY_SIZE;
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
        if (((name[name_len - 3] | 0x20) != 'n')
            || ((name[name_len - 2] | 0x20) != 'c')
            || ((name[name_len - 1] | 0x20) != 'a')) continue;
        if (fs < SIGIL_NCA_HEADER_SIZE) continue;

        uint8_t header[SIGIL_NCA_HEADER_SIZE];
        if (sigil_io_read_exact(io, data_start + fo, header, SIGIL_NCA_HEADER_SIZE) != SIGIL_OK) continue;
        int nrc = sigil_nca_title_from_raw_header(header, header_key_or_null,
                                                  out_title_id);
        if (nrc == SIGIL_OK) {
            rc = SIGIL_OK;
            break;
        }
        if (nrc == SIGIL_ERR_NEEDS_KEY) needs_key = true;
    }
    if (rc != SIGIL_OK && needs_key) rc = SIGIL_ERR_NEEDS_KEY;

    free(entries);
    free(string_table);
    return rc;
}

int sigil_extract_xci(const sigil_io *io, const sigil_options *opts,
                      sigil_result *out) {
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

#endif
