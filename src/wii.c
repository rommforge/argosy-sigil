/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <string.h>

/* GameCube/Wii ISO header layout:
 *   ISO: gameId at offset 0 (4 ASCII bytes)
 *   RVZ: 3-byte "RVZ" magic at offset 0, gameId at offset 0x58 (4 ASCII bytes)
 *
 * Save form is the hex encoding of the 4 ASCII bytes — e.g. "RZTE" -> "525A5445".
 * This matches Wii NAND save folder names and Dolphin's title-id tracking. */

static int extract_gameid(const sigil_io *io, uint64_t off, char raw[32], char canonical[32]) {
    uint8_t bytes[4];
    int rc = sigil_io_read_exact(io, off, bytes, sizeof(bytes));
    if (rc != SIGIL_OK) return rc;

    /* Validate ASCII printable region. GameCube/Wii game IDs are uppercase
     * letters and digits. */
    for (int i = 0; i < 4; i++) {
        if (!isalnum(bytes[i])) return SIGIL_ERR_NOT_FOUND;
    }

    /* raw = the 4 ASCII chars as-is. */
    memcpy(raw, bytes, 4);
    raw[4] = '\0';

    /* canonical = hex encoding (uppercase). */
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 4; i++) {
        canonical[i * 2]     = hex[(bytes[i] >> 4) & 0xF];
        canonical[i * 2 + 1] = hex[bytes[i] & 0xF];
    }
    canonical[8] = '\0';
    return SIGIL_OK;
}

static int extract_wii_or_gc(const sigil_io *io, sigil_platform platform,
                              sigil_result *out) {
    sigil_result_init(out);
    out->platform = platform;
    /* Wii is FOLDER_EXACT (one save folder per game in NAND); GameCube is
     * FILE_PREFIX (multiple .gci files per game share the gameId in their
     * basename). */
    out->usage = (platform == SIGIL_PLATFORM_WII)
        ? SIGIL_USAGE_FOLDER_EXACT
        : SIGIL_USAGE_FILE_PREFIX;

    /* Probe the first 3 bytes for RVZ magic. */
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
