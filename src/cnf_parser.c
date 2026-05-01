/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <string.h>

/* Find `needle` in `hay[0..hay_len]`. Like strstr but bounded. */
static const uint8_t *memmem_bounded(const uint8_t *hay, size_t hay_len,
                                      const char *needle, size_t needle_len) {
    if (needle_len == 0 || hay_len < needle_len) return NULL;
    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        if (memcmp(hay + i, needle, needle_len) == 0) return hay + i;
    }
    return NULL;
}

static bool starts_with(const uint8_t *p, size_t n, const char *prefix) {
    size_t plen = strlen(prefix);
    if (n < plen) return false;
    return memcmp(p, prefix, plen) == 0;
}

/* Test whether the 11 bytes at `p` form an LLLL[_.]NNN.NN serial pattern.
 * The trailing semicolon is checked separately by the caller because some
 * dumps put `;1` immediately after, others have whitespace before `;`. */
static bool match_serial_at(const uint8_t *p, size_t avail) {
    if (avail < 11) return false;
    if (!isupper(p[0]) || !isupper(p[1]) || !isupper(p[2]) || !isupper(p[3])) return false;
    if (p[4] != '_' && p[4] != '.') return false;
    if (!isdigit(p[5]) || !isdigit(p[6]) || !isdigit(p[7])) return false;
    if (p[8] != '.') return false;
    if (!isdigit(p[9]) || !isdigit(p[10])) return false;
    return true;
}

int sigil_cnf_parse_boot(const uint8_t *cnf, size_t len,
                         const char *boot_key,
                         char raw[32], char canonical[32]) {
    size_t key_len = strlen(boot_key);

    /* Scan every occurrence of the boot_key. */
    size_t off = 0;
    while (off + key_len <= len) {
        const uint8_t *hit = memmem_bounded(cnf + off, len - off, boot_key, key_len);
        if (!hit) return SIGIL_ERR_NOT_FOUND;
        size_t pos = (size_t)(hit - cnf) + key_len;
        off = pos;

        /* Must be followed by whitespace or `=`. Avoids false positives where
         * the BOOT scan matches inside "BOOT2". */
        if (pos >= len) continue;
        if (cnf[pos] != ' ' && cnf[pos] != '\t' && cnf[pos] != '=') continue;

        /* Skip whitespace. */
        while (pos < len && (cnf[pos] == ' ' || cnf[pos] == '\t')) pos++;
        if (pos >= len || cnf[pos] != '=') continue;
        pos++;
        while (pos < len && (cnf[pos] == ' ' || cnf[pos] == '\t')) pos++;

        /* Find end-of-line or end-of-buffer to bound the search. */
        size_t line_end = pos;
        while (line_end < len && cnf[line_end] != '\n' && cnf[line_end] != '\r') line_end++;

        /* Scan within [pos, line_end) for an LLLL[_.]NNN.NN pattern.
         * This handles subdirectories in the boot path
         * (e.g. `cdrom:\MARL\SLUS_010.73;1` for Rhapsody) that argosy's
         * fixed-position regex can't match. */
        size_t serial_pos = (size_t)-1;
        for (size_t i = pos; i + 11 <= line_end; i++) {
            if (match_serial_at(cnf + i, line_end - i)) {
                serial_pos = i;
                break;
            }
        }
        if (serial_pos == (size_t)-1) continue;

        /* raw: 11-char as-is. */
        memcpy(raw, cnf + serial_pos, 11);
        raw[11] = '\0';
        /* canonical: LLLL-NNNNN (10 chars). */
        memcpy(canonical, cnf + serial_pos, 4);
        canonical[4] = '-';
        canonical[5] = cnf[serial_pos + 5];
        canonical[6] = cnf[serial_pos + 6];
        canonical[7] = cnf[serial_pos + 7];
        canonical[8] = cnf[serial_pos + 9];
        canonical[9] = cnf[serial_pos + 10];
        canonical[10] = '\0';
        return SIGIL_OK;
    }
    return SIGIL_ERR_NOT_FOUND;
}
