// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

#define XEX_MIN_HEADER          0x18
#define XEX_MAX_OPT_HEADERS     128
#define XEX_HEADER_EXEC_INFO    0x00040006u
#define XEX_TITLE_ID_OFFSET     0x0C

static bool xex_magic_ok(const uint8_t *p) {
    return (p[0] == 'X' && p[1] == 'E' && p[2] == 'X'
            && (p[3] == '2' || p[3] == '1'));
}

int sigil_extract_xbox360(const sigil_io *io, const char *filename_hint,
                          const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;

    sigil_result_init(out);
    out->platform     = SIGIL_PLATFORM_XBOX360;
    out->usage        = SIGIL_USAGE_FOLDER_EXACT;
    out->experimental = 1;

    if (!io || !io->read || !io->size) return SIGIL_ERR_INVALID_ARG;

    uint8_t header[XEX_MIN_HEADER];
    if (sigil_io_read_exact(io, 0, header, sizeof(header)) != SIGIL_OK) {
        return SIGIL_ERR_IO;
    }
    if (!xex_magic_ok(header)) return SIGIL_ERR_UNSUPPORTED_FORMAT;

    uint32_t header_count = sigil_read_be32(header + 0x14);
    if (header_count == 0 || header_count > XEX_MAX_OPT_HEADERS) {
        return SIGIL_ERR_NOT_FOUND;
    }

    for (uint32_t i = 0; i < header_count; i++) {
        uint8_t entry[8];
        uint64_t entry_off = (uint64_t)XEX_MIN_HEADER + (uint64_t)i * 8;
        if (sigil_io_read_exact(io, entry_off, entry, sizeof(entry)) != SIGIL_OK) {
            return SIGIL_ERR_IO;
        }
        uint32_t key   = sigil_read_be32(entry);
        uint32_t value = sigil_read_be32(entry + 4);
        if (key != XEX_HEADER_EXEC_INFO) continue;

        uint8_t tid_bytes[4];
        if (sigil_io_read_exact(io, (uint64_t)value + XEX_TITLE_ID_OFFSET,
                                 tid_bytes, sizeof(tid_bytes)) != SIGIL_OK) {
            return SIGIL_ERR_IO;
        }
        sigil_hex_encode_4(tid_bytes, out->title_id);
        memcpy(out->raw_serial, out->title_id, strlen(out->title_id) + 1);
        out->source = SIGIL_SOURCE_BINARY;
        return SIGIL_OK;
    }
    return SIGIL_ERR_NOT_FOUND;
}
