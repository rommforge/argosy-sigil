// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

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

static bool match_serial_at(const uint8_t *p, size_t avail) {
    if (avail < 11) return false;
    if (!sigil_is_upper(p[0]) || !sigil_is_upper(p[1])
        || !sigil_is_upper(p[2]) || !sigil_is_upper(p[3])) return false;
    if (p[4] != '_' && p[4] != '.') return false;
    if (!sigil_is_dig(p[5]) || !sigil_is_dig(p[6]) || !sigil_is_dig(p[7])) return false;
    if (p[8] != '.') return false;
    if (!sigil_is_dig(p[9]) || !sigil_is_dig(p[10])) return false;
    return true;
}

int sigil_cnf_parse_boot(const uint8_t *cnf, size_t len,
                         const char *boot_key,
                         char raw[32], char canonical[32]) {
    size_t key_len = strlen(boot_key);

    size_t off = 0;
    while (off + key_len <= len) {
        const uint8_t *hit = memmem_bounded(cnf + off, len - off, boot_key, key_len);
        if (!hit) return SIGIL_ERR_NOT_FOUND;
        size_t pos = (size_t)(hit - cnf) + key_len;
        off = pos;

        /* Reject matches embedded inside longer tokens like "BOOT2" when the
         * caller asked for "BOOT". */
        if (pos >= len) continue;
        if (cnf[pos] != ' ' && cnf[pos] != '\t' && cnf[pos] != '=') continue;

        while (pos < len && (cnf[pos] == ' ' || cnf[pos] == '\t')) pos++;
        if (pos >= len || cnf[pos] != '=') continue;
        pos++;
        while (pos < len && (cnf[pos] == ' ' || cnf[pos] == '\t')) pos++;

        if (starts_with(cnf + pos, len - pos, "cdrom0:")) pos += 7;
        else if (starts_with(cnf + pos, len - pos, "cdrom:")) pos += 6;
        if (pos < len && cnf[pos] == '\\') pos++;

        size_t line_end = pos;
        while (line_end < len && cnf[line_end] != '\n' && cnf[line_end] != '\r') line_end++;

        /* Scan the bounded line for the LLLL[_.]NNN.NN pattern; this handles
         * subdirectory paths like cdrom:\MARL\SLUS_010.73;1 (Rhapsody). */
        size_t serial_pos = (size_t)-1;
        for (size_t i = pos; i + 11 <= line_end; i++) {
            if (match_serial_at(cnf + i, line_end - i)) {
                serial_pos = i;
                break;
            }
        }
        if (serial_pos == (size_t)-1) continue;

        memcpy(raw, cnf + serial_pos, 11);
        raw[11] = '\0';
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
