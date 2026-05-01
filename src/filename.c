/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <string.h>

/* Hand-rolled scanners replacing argosy's per-platform regex fallbacks.
 * Patterns target the No-Intro / Redump community naming conventions:
 *   PSP / PSVita: `[ULUS10064]` or `(ULUS10064)` or filename-prefix
 *   PSX / PS2:    `[SLUS-12345]` or `(SLUS-12345)`
 *   Switch / 3DS: `[0100ABCD12345000]` or 8-hex variant for 3DS short ids
 *   Wii U:        `[00050000101F2800]` 16-hex; canonical = last 8 chars */

/* Extract the basename (no leading path) and strip the extension. */
static void stem_of(const char *filename_hint, char out[256]) {
    out[0] = '\0';
    if (!filename_hint) return;

    /* Skip directory components. */
    const char *base = filename_hint;
    const char *slash = strrchr(filename_hint, '/');
    if (slash) base = slash + 1;
#ifdef _WIN32
    const char *bs = strrchr(filename_hint, '\\');
    if (bs && bs >= base) base = bs + 1;
#endif

    size_t len = strlen(base);
    /* Drop everything from the last '.' to end. */
    const char *dot = strrchr(base, '.');
    if (dot && dot > base) len = (size_t)(dot - base);
    if (len > 255) len = 255;
    memcpy(out, base, len);
    out[len] = '\0';
}

static bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
static bool is_dig(char c)   { return c >= '0' && c <= '9'; }
static bool is_hex(char c)   {
    return is_dig(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/* Match `[A-Z]{4}\d{5}` at `s + i`. Returns true and writes 9 chars to `out` on hit. */
static bool match_psp_id(const char *s, size_t len, size_t i, char out[10]) {
    if (i + 9 > len) return false;
    for (int k = 0; k < 4; k++) if (!is_upper(s[i + k])) return false;
    for (int k = 4; k < 9; k++) if (!is_dig(s[i + k])) return false;
    memcpy(out, s + i, 9);
    out[9] = '\0';
    return true;
}

/* Match `[A-Z]{4}-\d{5}` at `s + i`. */
static bool match_dashed_serial(const char *s, size_t len, size_t i, char out[11]) {
    if (i + 10 > len) return false;
    for (int k = 0; k < 4; k++) if (!is_upper(s[i + k])) return false;
    if (s[i + 4] != '-') return false;
    for (int k = 5; k < 10; k++) if (!is_dig(s[i + k])) return false;
    memcpy(out, s + i, 10);
    out[10] = '\0';
    return true;
}

/* Match `[0-9A-Fa-f]{n}` at `s + i`. */
static bool match_hex_run(const char *s, size_t len, size_t i, size_t n, char *out) {
    if (i + n > len) return false;
    for (size_t k = 0; k < n; k++) if (!is_hex(s[i + k])) return false;
    for (size_t k = 0; k < n; k++) {
        char c = s[i + k];
        out[k] = (c >= 'a' && c <= 'f') ? (char)(c - 32) : c;  /* uppercase */
    }
    out[n] = '\0';
    return true;
}

/* Helper: confirm the byte at offset is a delimiter we accept around bracketed tags. */
static bool is_open_brk(char c)  { return c == '['; }
static bool is_close_brk(char c) { return c == ']'; }
static bool is_open_par(char c)  { return c == '('; }
static bool is_close_par(char c) { return c == ')'; }

/* ---- Per-platform scanners ---------------------------------------------- */

static int try_psp(const char *stem, size_t len, sigil_result *out) {
    char id[10];
    /* [ULUS10064] */
    for (size_t i = 0; i + 11 <= len; i++) {
        if (is_open_brk(stem[i]) && match_psp_id(stem, len, i + 1, id)
            && is_close_brk(stem[i + 10])) {
            memcpy(out->title_id, id, 10);
            memcpy(out->raw_serial, id, 10);
            return SIGIL_OK;
        }
    }
    /* (ULUS10064) */
    for (size_t i = 0; i + 11 <= len; i++) {
        if (is_open_par(stem[i]) && match_psp_id(stem, len, i + 1, id)
            && is_close_par(stem[i + 10])) {
            memcpy(out->title_id, id, 10);
            memcpy(out->raw_serial, id, 10);
            return SIGIL_OK;
        }
    }
    /* ^ULUS10064 */
    if (match_psp_id(stem, len, 0, id)) {
        memcpy(out->title_id, id, 10);
        memcpy(out->raw_serial, id, 10);
        return SIGIL_OK;
    }
    return SIGIL_ERR_NOT_FOUND;
}

static int try_dashed_serial(const char *stem, size_t len, sigil_result *out) {
    char id[11];
    /* [SLUS-12345] */
    for (size_t i = 0; i + 12 <= len; i++) {
        if (is_open_brk(stem[i]) && match_dashed_serial(stem, len, i + 1, id)
            && is_close_brk(stem[i + 11])) {
            memcpy(out->title_id, id, 11);
            memcpy(out->raw_serial, id, 11);
            return SIGIL_OK;
        }
    }
    /* (SLUS-12345) */
    for (size_t i = 0; i + 12 <= len; i++) {
        if (is_open_par(stem[i]) && match_dashed_serial(stem, len, i + 1, id)
            && is_close_par(stem[i + 11])) {
            memcpy(out->title_id, id, 11);
            memcpy(out->raw_serial, id, 11);
            return SIGIL_OK;
        }
    }
    /* Bare LLLL-NNNNN anywhere — argosy's regex is `\[?...\]?`, accepting
     * unbracketed too, so we scan as a fallback. */
    for (size_t i = 0; i + 10 <= len; i++) {
        if (match_dashed_serial(stem, len, i, id)) {
            memcpy(out->title_id, id, 11);
            memcpy(out->raw_serial, id, 11);
            return SIGIL_OK;
        }
    }
    return SIGIL_ERR_NOT_FOUND;
}

static int try_hex16(const char *stem, size_t len, sigil_result *out, bool require_01_prefix) {
    char id[17];
    /* [0100ABCD12345000] */
    for (size_t i = 0; i + 18 <= len; i++) {
        if (is_open_brk(stem[i]) && match_hex_run(stem, len, i + 1, 16, id)
            && is_close_brk(stem[i + 17])) {
            if (require_01_prefix && !(id[0] == '0' && id[1] == '1')) continue;
            memcpy(out->title_id, id, 17);
            memcpy(out->raw_serial, id, 17);
            return SIGIL_OK;
        }
    }
    /* (0100ABCD12345000) */
    for (size_t i = 0; i + 18 <= len; i++) {
        if (is_open_par(stem[i]) && match_hex_run(stem, len, i + 1, 16, id)
            && is_close_par(stem[i + 17])) {
            if (require_01_prefix && !(id[0] == '0' && id[1] == '1')) continue;
            memcpy(out->title_id, id, 17);
            memcpy(out->raw_serial, id, 17);
            return SIGIL_OK;
        }
    }
    /* Switch suffix form: [-_]<16-hex>$ */
    if (require_01_prefix && len >= 17) {
        size_t i = len - 16;
        if (i > 0 && (stem[i - 1] == '-' || stem[i - 1] == '_')
            && match_hex_run(stem, len, i, 16, id)
            && id[0] == '0' && id[1] == '1') {
            memcpy(out->title_id, id, 17);
            memcpy(out->raw_serial, id, 17);
            return SIGIL_OK;
        }
    }
    return SIGIL_ERR_NOT_FOUND;
}

static int try_3ds(const char *stem, size_t len, sigil_result *out) {
    /* Try 16-hex first (full title id, 0004... required). */
    char id16[17];
    for (size_t i = 0; i + 18 <= len; i++) {
        if (is_open_brk(stem[i]) && match_hex_run(stem, len, i + 1, 16, id16)
            && is_close_brk(stem[i + 17])) {
            if (id16[0] == '0' && id16[1] == '0' && id16[2] == '0' && id16[3] == '4') {
                memcpy(out->title_id, id16, 17);
                memcpy(out->raw_serial, id16, 17);
                return SIGIL_OK;
            }
        }
    }
    /* 8-hex short form: [001B5000] */
    char id8[9];
    for (size_t i = 0; i + 10 <= len; i++) {
        if (is_open_brk(stem[i]) && match_hex_run(stem, len, i + 1, 8, id8)
            && is_close_brk(stem[i + 9])) {
            memcpy(out->title_id, id8, 9);
            memcpy(out->raw_serial, id8, 9);
            return SIGIL_OK;
        }
    }
    /* Parenthesized 8-or-16 hex variant. */
    for (size_t i = 0; i + 10 <= len; i++) {
        if (is_open_par(stem[i]) && match_hex_run(stem, len, i + 1, 8, id8)
            && is_close_par(stem[i + 9])) {
            memcpy(out->title_id, id8, 9);
            memcpy(out->raw_serial, id8, 9);
            return SIGIL_OK;
        }
    }
    return SIGIL_ERR_NOT_FOUND;
}

static int try_wiiu(const char *stem, size_t len, sigil_result *out) {
    /* Wii U filenames carry the FULL 16-hex title id in brackets:
     *   [00050000101F2800]
     * Canonical save-form is the LAST 8 chars (`101F2800`) — that's what the
     * platform's save system uses. raw_serial preserves the full 16-hex. */
    char id16[17];
    for (size_t i = 0; i + 18 <= len; i++) {
        if (is_open_brk(stem[i]) && match_hex_run(stem, len, i + 1, 16, id16)
            && is_close_brk(stem[i + 17])) {
            if (id16[0] == '0' && id16[1] == '0' && id16[2] == '0' && id16[3] == '5') {
                memcpy(out->raw_serial, id16, 17);
                memcpy(out->title_id, id16 + 8, 9);  /* last 8 + NUL */
                return SIGIL_OK;
            }
        }
    }
    /* 8-hex variant: [101F2800] — argosy accepts this too. */
    char id8[9];
    for (size_t i = 0; i + 10 <= len; i++) {
        if (is_open_brk(stem[i]) && match_hex_run(stem, len, i + 1, 8, id8)
            && is_close_brk(stem[i + 9])) {
            memcpy(out->title_id, id8, 9);
            memcpy(out->raw_serial, id8, 9);
            return SIGIL_OK;
        }
    }
    /* Parenthesized 8-hex. */
    for (size_t i = 0; i + 10 <= len; i++) {
        if (is_open_par(stem[i]) && match_hex_run(stem, len, i + 1, 8, id8)
            && is_close_par(stem[i + 9])) {
            memcpy(out->title_id, id8, 9);
            memcpy(out->raw_serial, id8, 9);
            return SIGIL_OK;
        }
    }
    return SIGIL_ERR_NOT_FOUND;
}

/* ---- Public entry ------------------------------------------------------- */

int sigil_filename_fallback(const char *filename_hint,
                            sigil_platform platform,
                            sigil_result *out) {
    if (!filename_hint || !out) return SIGIL_ERR_INVALID_ARG;

    char stem[256];
    stem_of(filename_hint, stem);
    size_t len = strlen(stem);
    if (len == 0) return SIGIL_ERR_NOT_FOUND;

    sigil_result_init(out);
    out->platform = platform;
    out->source = SIGIL_SOURCE_FILENAME;

    int rc = SIGIL_ERR_NOT_FOUND;
    switch (platform) {
    case SIGIL_PLATFORM_PSP:
        out->usage = SIGIL_USAGE_FOLDER_PREFIX;
        rc = try_psp(stem, len, out);
        break;
    case SIGIL_PLATFORM_PSVITA:
        out->usage = SIGIL_USAGE_FOLDER_EXACT;
        rc = try_psp(stem, len, out);  /* same shape: LLLL\d{5} */
        break;
    case SIGIL_PLATFORM_PSX:
        out->usage = SIGIL_USAGE_FILE_PREFIX;
        rc = try_dashed_serial(stem, len, out);
        break;
    case SIGIL_PLATFORM_PS2:
        out->usage = SIGIL_USAGE_FOLDER_EXACT;
        rc = try_dashed_serial(stem, len, out);
        break;
    case SIGIL_PLATFORM_SWITCH:
        out->usage = SIGIL_USAGE_FOLDER_EXACT;
        rc = try_hex16(stem, len, out, true);
        break;
    case SIGIL_PLATFORM_3DS:
        out->usage = SIGIL_USAGE_FOLDER_EXACT;
        rc = try_3ds(stem, len, out);
        break;
    case SIGIL_PLATFORM_WIIU:
        out->usage = SIGIL_USAGE_FOLDER_EXACT;
        rc = try_wiiu(stem, len, out);
        break;
    case SIGIL_PLATFORM_WII:
    case SIGIL_PLATFORM_GAMECUBE:
        /* Wii / GC filenames typically carry the 4-letter ASCII gameId in
         * brackets, e.g. `[RZTE]` or `[GZLE]`. No filename fallback in argosy
         * for these (binary parse always succeeds), but we offer the same
         * shape here for completeness. */
        out->usage = (platform == SIGIL_PLATFORM_WII)
            ? SIGIL_USAGE_FOLDER_EXACT
            : SIGIL_USAGE_FILE_PREFIX;
        for (size_t i = 0; i + 6 <= len; i++) {
            if (is_open_brk(stem[i])
                && is_upper(stem[i+1]) && is_upper(stem[i+2])
                && (is_upper(stem[i+3]) || is_dig(stem[i+3]))
                && (is_upper(stem[i+4]) || is_dig(stem[i+4]))
                && is_close_brk(stem[i+5])) {
                /* raw = the 4 ASCII bytes; canonical = hex of those bytes. */
                memcpy(out->raw_serial, stem + i + 1, 4);
                out->raw_serial[4] = '\0';
                static const char hex[] = "0123456789ABCDEF";
                for (int k = 0; k < 4; k++) {
                    uint8_t b = (uint8_t)stem[i + 1 + k];
                    out->title_id[k * 2]     = hex[(b >> 4) & 0xF];
                    out->title_id[k * 2 + 1] = hex[b & 0xF];
                }
                out->title_id[8] = '\0';
                rc = SIGIL_OK;
                break;
            }
        }
        break;
    case SIGIL_PLATFORM_AUTO:
    default:
        /* AUTO fallback: try Switch (most discriminating), then Wii U, then
         * dashed-serial PSX/PS2, then PSP. The platform field on the result
         * is set to the matched candidate. */
        out->platform = SIGIL_PLATFORM_SWITCH;
        out->usage = SIGIL_USAGE_FOLDER_EXACT;
        if ((rc = try_hex16(stem, len, out, true)) == SIGIL_OK) break;

        out->platform = SIGIL_PLATFORM_WIIU;
        if ((rc = try_wiiu(stem, len, out)) == SIGIL_OK) break;

        out->platform = SIGIL_PLATFORM_PS2;
        out->usage = SIGIL_USAGE_FOLDER_EXACT;
        if ((rc = try_dashed_serial(stem, len, out)) == SIGIL_OK) break;

        out->platform = SIGIL_PLATFORM_PSP;
        out->usage = SIGIL_USAGE_FOLDER_PREFIX;
        rc = try_psp(stem, len, out);
        break;
    }

    return rc;
}
