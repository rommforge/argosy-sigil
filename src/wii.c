// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

/* GameCube/Wii ISO header: 4-byte ASCII gameId at offset 0 (raw ISO) or
 * 0x58 (RVZ). Save form is the hex encoding of the 4 ASCII bytes. */

static int extract_gameid(const sigil_io *io, uint64_t off, char raw[32], char canonical[32]) {
    uint8_t bytes[4];
    int rc = sigil_io_read_exact(io, off, bytes, sizeof(bytes));
    if (rc != SIGIL_OK) return rc;

    for (int i = 0; i < 4; i++) {
        if (!(sigil_is_upper((char)bytes[i]) || sigil_is_dig((char)bytes[i]))) {
            return SIGIL_ERR_NOT_FOUND;
        }
    }

    memcpy(raw, bytes, 4);
    raw[4] = '\0';
    sigil_hex_encode_4(bytes, canonical);
    return SIGIL_OK;
}

static int extract_wii_or_gc(const sigil_io *io, sigil_platform platform,
                              sigil_result *out) {
    sigil_result_init(out);
    out->platform = platform;
    out->usage = (platform == SIGIL_PLATFORM_WII)
        ? SIGIL_USAGE_FOLDER_EXACT
        : SIGIL_USAGE_FILE_PREFIX;

    uint8_t magic[3];
    int rc = sigil_io_read_exact(io, 0, magic, sizeof(magic));
    if (rc != SIGIL_OK) return rc;

    uint64_t id_off = (memcmp(magic, "RVZ", 3) == 0) ? 0x58 : 0x00;
    rc = extract_gameid(io, id_off, out->raw_serial, out->title_id);
    if (rc != SIGIL_OK) return rc;

    out->source = SIGIL_SOURCE_BINARY;
    return SIGIL_OK;
}

int sigil_extract_wii(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;
    return extract_wii_or_gc(io, SIGIL_PLATFORM_WII, out);
}

int sigil_extract_gamecube(const sigil_io *io, const char *filename_hint,
                           const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;
    return extract_wii_or_gc(io, SIGIL_PLATFORM_GAMECUBE, out);
}
