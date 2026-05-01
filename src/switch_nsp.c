// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

#if SIGIL_WITH_SWITCH

#include <stdlib.h>

#define MAX_SWITCH_FILE_COUNT  4096
#define MAX_SWITCH_STRING_TABLE (1024 * 1024)

int sigil_extract_xci(const sigil_io *io, const sigil_options *opts,
                      sigil_result *out);

static int scan_string_table_for_nca_title(const uint8_t *table, size_t len,
                                            char out_title_id[17]) {
    if (len < 20) return SIGIL_ERR_NOT_FOUND;
    for (size_t i = 0; i + 20 <= len; i++) {
        bool ok = true;
        for (int k = 0; k < 16; k++) {
            if (!sigil_is_hex((char)table[i + k])) { ok = false; break; }
        }
        if (!ok) continue;
        if (table[i + 16] != '.' ||
            (table[i + 17] != 'n' && table[i + 17] != 'N') ||
            (table[i + 18] != 'c' && table[i + 18] != 'C') ||
            (table[i + 19] != 'a' && table[i + 19] != 'A')) continue;

        char tmp[17];
        for (int k = 0; k < 16; k++) tmp[k] = sigil_to_upper((char)table[i + k]);
        tmp[16] = '\0';
        if (tmp[0] == '0' && tmp[1] == '1') {
            memcpy(out_title_id, tmp, 17);
            return SIGIL_OK;
        }
    }
    return SIGIL_ERR_NOT_FOUND;
}

int sigil_pfs0_extract_title_id(const sigil_io *io, uint64_t partition_off,
                                 const uint8_t *header_key_or_null,
                                 char out_title_id[17]) {
    uint8_t hdr[16];
    int rc = sigil_io_read_exact(io, partition_off, hdr, 16);
    if (rc != SIGIL_OK) return rc;
    if (memcmp(hdr, "PFS0", 4) != 0) return SIGIL_ERR_NOT_FOUND;

    uint32_t file_count        = sigil_read_le32(hdr + 4);
    uint32_t string_table_size = sigil_read_le32(hdr + 8);

    if (file_count > MAX_SWITCH_FILE_COUNT || string_table_size > MAX_SWITCH_STRING_TABLE) {
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

    /* Cheap path first: NCAs whose filenames are themselves the 16-hex
     * title id (decrypted dumps and homebrew). */
    if (scan_string_table_for_nca_title(string_table, string_table_size, out_title_id) == SIGIL_OK) {
        free(entries);
        free(string_table);
        return SIGIL_OK;
    }

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

        uint32_t name_end = name_offset;
        while (name_end < string_table_size && string_table[name_end] != 0) name_end++;
        size_t name_len = name_end - name_offset;
        if (name_len < 4) continue;

        const char *name = (const char *)string_table + name_offset;
        if (name[name_len - 4] != '.' ||
            (name[name_len - 3] | 0x20) != 'n' ||
            (name[name_len - 2] | 0x20) != 'c' ||
            (name[name_len - 1] | 0x20) != 'a') continue;

        if (file_size < SIGIL_NCA_HEADER_SIZE) continue;

        uint8_t header[SIGIL_NCA_HEADER_SIZE];
        if (sigil_io_read_exact(io, data_start + file_offset, header,
                                 SIGIL_NCA_HEADER_SIZE) != SIGIL_OK) continue;

        sigil_aes_xts_decrypt_nintendo(header_key_or_null, 0, header, SIGIL_NCA_HEADER_SIZE);
        if (sigil_nca_extract_title_id(header, out_title_id) == SIGIL_OK) {
            rc = SIGIL_OK;
            break;
        }
    }

    free(entries);
    free(string_table);
    return rc;
}

int sigil_extract_switch(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    if (!io) return SIGIL_ERR_INVALID_ARG;

    sigil_result_init(out);
    out->platform = SIGIL_PLATFORM_SWITCH;
    out->usage = SIGIL_USAGE_FOLDER_EXACT;

    uint8_t magic[4];
    int rc = sigil_io_read_exact(io, 0, magic, 4);
    if (rc != SIGIL_OK) return rc;

    uint8_t hkey[32];
    bool have_key = (opts && opts->support
                     && sigil_resolve_header_key(opts->support, hkey) == SIGIL_OK);

    if (memcmp(magic, "PFS0", 4) == 0) {
        char tid[17];
        rc = sigil_pfs0_extract_title_id(io, 0, have_key ? hkey : NULL, tid);
        if (rc != SIGIL_OK) return rc;
        memcpy(out->title_id, tid, 17);
        memcpy(out->raw_serial, tid, 17);
        out->source = SIGIL_SOURCE_BINARY;
        return SIGIL_OK;
    }

    return sigil_extract_xci(io, opts, out);
}

#endif
