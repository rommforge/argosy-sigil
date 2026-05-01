/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

#if SIGIL_WITH_WIIU

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Port of argosy's ZArchiveReader.kt — only the metadata path needed for
 * Wii U title-ID extraction. The file tree and name table are stored
 * uncompressed in the archive (only file *contents* go through zstd), so
 * this milestone avoids the zstd dependency entirely. */

#define WUA_MAGIC         0x169F52D6u
#define WUA_VERSION_1     0x61BF3A01u
#define WUA_FOOTER_SIZE   144
#define WUA_FILE_ENTRY    16

/* Footer fields we care about (big-endian); all u64 unless noted. Order:
 *   0x00 compressed_offset
 *   0x08 compressed_size
 *   0x10 offset_records_offset
 *   0x18 offset_records_size
 *   0x20 names_offset
 *   0x28 names_size
 *   0x30 file_tree_offset
 *   0x38 file_tree_size
 *   0x40..0x60 metaDir + metaData (unused)
 *   0x60..0x80 SHA-256 (32 bytes)
 *   0x80 total_size (u64)
 *   0x88 version (u32)
 *   0x8C magic (u32)
 */

typedef struct {
    uint64_t names_offset, names_size;
    uint64_t file_tree_offset, file_tree_size;
} wua_footer;

static int read_footer(const sigil_io *io, wua_footer *out) {
    int64_t total = io->size(io->ctx);
    if (total < WUA_FOOTER_SIZE) return SIGIL_ERR_NOT_FOUND;

    uint8_t buf[WUA_FOOTER_SIZE];
    int rc = sigil_io_read_exact(io, (uint64_t)(total - WUA_FOOTER_SIZE),
                                  buf, WUA_FOOTER_SIZE);
    if (rc != SIGIL_OK) return rc;

    uint32_t version = sigil_read_be32(buf + 0x88);
    uint32_t magic   = sigil_read_be32(buf + 0x8C);
    if (magic != WUA_MAGIC || version != WUA_VERSION_1) return SIGIL_ERR_NOT_FOUND;

    out->names_offset     = sigil_read_be64(buf + 0x20);
    out->names_size       = sigil_read_be64(buf + 0x28);
    out->file_tree_offset = sigil_read_be64(buf + 0x30);
    out->file_tree_size   = sigil_read_be64(buf + 0x38);
    return SIGIL_OK;
}

/* Read a name from the name table given a 1-byte length prefix. */
static size_t read_name(const uint8_t *names, size_t names_len,
                        uint32_t offset, char *out, size_t out_size) {
    if (offset >= names_len) return 0;
    size_t len = names[offset];
    if (offset + 1 + len > names_len) return 0;
    if (len + 1 > out_size) return 0;
    memcpy(out, names + offset + 1, len);
    out[len] = '\0';
    return len;
}

static bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Match `00050000<8 hex>` at the start of `name`. */
static bool match_wiiu_title_dir(const char *name, char canonical[9], char raw[17]) {
    if (strlen(name) < 16) return false;
    if (memcmp(name, "00050000", 8) != 0) return false;
    for (int i = 0; i < 8; i++) {
        if (!is_hex_char(name[8 + i])) return false;
    }
    /* canonical = last 8 chars, uppercase */
    for (int i = 0; i < 8; i++) {
        char c = name[8 + i];
        canonical[i] = (c >= 'a' && c <= 'f') ? (char)(c - 32) : c;
    }
    canonical[8] = '\0';
    /* raw = full 16, uppercase */
    for (int i = 0; i < 16; i++) {
        char c = name[i];
        raw[i] = (c >= 'a' && c <= 'f') ? (char)(c - 32) : c;
    }
    raw[16] = '\0';
    return true;
}

int sigil_extract_wiiu(const sigil_io *io, const char *filename_hint,
                       const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;
    if (!io) return SIGIL_ERR_INVALID_ARG;

    sigil_result_init(out);
    out->platform = SIGIL_PLATFORM_WIIU;
    out->usage = SIGIL_USAGE_FOLDER_EXACT;

    wua_footer ft;
    int rc = read_footer(io, &ft);
    if (rc != SIGIL_OK) return rc;

    /* Cap the metadata reads to keep memory bounded. */
    if (ft.names_size > 16 * 1024 * 1024 || ft.file_tree_size > 16 * 1024 * 1024) {
        return SIGIL_ERR_NOT_FOUND;
    }

    uint8_t *names = (uint8_t *)malloc(ft.names_size);
    if (!names) return SIGIL_ERR_OOM;
    rc = sigil_io_read_exact(io, ft.names_offset, names, (size_t)ft.names_size);
    if (rc != SIGIL_OK) { free(names); return rc; }

    uint8_t *tree = (uint8_t *)malloc(ft.file_tree_size);
    if (!tree) { free(names); return SIGIL_ERR_OOM; }
    rc = sigil_io_read_exact(io, ft.file_tree_offset, tree, (size_t)ft.file_tree_size);
    if (rc != SIGIL_OK) { free(names); free(tree); return rc; }

    /* Root entry is at index 0; expected to be a directory. The WUA file-
     * tree entry layout is 16 bytes:
     *   directory:  [name_off | flag] [child_start_idx] [child_count] [reserved]
     *   file:       [name_off | flag] [offset_lo] [size_lo] [size_hi|offset_hi]
     * `flag` bit (top of name_off u32) is 0 for directory, 1 for file. */
    if (ft.file_tree_size < WUA_FILE_ENTRY) {
        free(names); free(tree);
        return SIGIL_ERR_NOT_FOUND;
    }

    uint32_t root_name_off_and_flag = sigil_read_be32(tree);
    bool root_is_dir = (root_name_off_and_flag & 0x80000000u) == 0;
    if (!root_is_dir) {
        free(names); free(tree);
        return SIGIL_ERR_NOT_FOUND;
    }
    uint32_t root_start = sigil_read_be32(tree + 4);
    uint32_t root_count = sigil_read_be32(tree + 8);

    uint32_t total_nodes = (uint32_t)(ft.file_tree_size / WUA_FILE_ENTRY);

    rc = SIGIL_ERR_NOT_FOUND;
    for (uint32_t i = 0; i < root_count; i++) {
        uint32_t idx = root_start + i;
        if (idx >= total_nodes) break;

        const uint8_t *e = tree + (size_t)idx * WUA_FILE_ENTRY;
        uint32_t nf = sigil_read_be32(e);
        bool is_dir = (nf & 0x80000000u) == 0;
        if (!is_dir) continue;

        uint32_t name_off = nf & 0x7FFFFFFFu;
        char name[256];
        size_t got = read_name(names, (size_t)ft.names_size, name_off, name, sizeof(name));
        if (got == 0) continue;

        char canonical[9], raw[17];
        if (match_wiiu_title_dir(name, canonical, raw)) {
            memcpy(out->title_id,   canonical, 9);
            memcpy(out->raw_serial, raw,       17);
            out->source = SIGIL_SOURCE_BINARY;
            rc = SIGIL_OK;
            break;
        }
    }

    free(names);
    free(tree);
    return rc;
}

#endif /* SIGIL_WITH_WIIU */
