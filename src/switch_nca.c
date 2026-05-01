/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

#if SIGIL_WITH_SWITCH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NCA_MAGIC_OFFSET      0x200
#define NCA_PROGRAM_ID_OFFSET 0x210
#define NCA_RIGHTS_ID_OFFSET  0x230

static void hex16_upper(const uint8_t *src8, char out[17], bool reverse) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        uint8_t b = reverse ? src8[7 - i] : src8[i];
        out[i * 2]     = hex[(b >> 4) & 0xF];
        out[i * 2 + 1] = hex[b & 0xF];
    }
    out[16] = '\0';
}

int sigil_resolve_header_key(const sigil_support *sup, uint8_t out[32]) {
    if (!sup) return SIGIL_ERR_NEEDS_KEY;

    if (sup->switch_header_key) {
        memcpy(out, sup->switch_header_key, 32);
        return SIGIL_OK;
    }
    if (sup->switch_prod_keys_text && sup->switch_prod_keys_text_len > 0) {
        return sigil_decode_header_key_from_text(
            sup->switch_prod_keys_text, sup->switch_prod_keys_text_len, out);
    }
    if (sup->switch_prod_keys_path) {
        return sigil_load_header_key_from_prod_keys(sup->switch_prod_keys_path, out);
    }
    return SIGIL_ERR_NEEDS_KEY;
}

int sigil_nca_extract_title_id(const uint8_t *decrypted_header, char out_title_id[17]) {
    /* Validate NCA3 magic at offset 0x200. */
    if (memcmp(decrypted_header + NCA_MAGIC_OFFSET, "NCA3", 4) != 0) {
        return SIGIL_ERR_NOT_FOUND;
    }

    /* Try program_id at 0x210: 8 bytes little-endian, output as
     * uppercase hex with bytes reversed. Validity gate: must start with
     * "01" (retail title-id prefix) and be 16 chars. */
    char candidate[17];
    hex16_upper(decrypted_header + NCA_PROGRAM_ID_OFFSET, candidate, true);
    if (candidate[0] == '0' && candidate[1] == '1') {
        memcpy(out_title_id, candidate, 17);
        return SIGIL_OK;
    }

    /* Fall back to rights_id at 0x230: first 8 bytes are the title id,
     * big-endian as-is (no reversal). */
    const uint8_t *rid = decrypted_header + NCA_RIGHTS_ID_OFFSET;
    bool nonzero = false;
    for (int i = 0; i < 16; i++) { if (rid[i] != 0) { nonzero = true; break; } }
    if (nonzero) {
        hex16_upper(rid, candidate, false);
        if (candidate[0] == '0' && candidate[1] == '1') {
            memcpy(out_title_id, candidate, 17);
            return SIGIL_OK;
        }
    }

    return SIGIL_ERR_NOT_FOUND;
}

#endif /* SIGIL_WITH_SWITCH */
