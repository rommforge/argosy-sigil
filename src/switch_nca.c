// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

#if SIGIL_WITH_SWITCH

#define NCA_MAGIC_OFFSET      0x200
#define NCA_PROGRAM_ID_OFFSET 0x210
#define NCA_RIGHTS_ID_OFFSET  0x230

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
    if (memcmp(decrypted_header + NCA_MAGIC_OFFSET, "NCA3", 4) != 0) {
        return SIGIL_ERR_NOT_FOUND;
    }

    /* program_id at 0x210: 8 bytes LE, byte-reverse to hex with "01" gate. */
    char candidate[17];
    sigil_hex_encode_8(decrypted_header + NCA_PROGRAM_ID_OFFSET, candidate, true);
    if (candidate[0] == '0' && candidate[1] == '1') {
        memcpy(out_title_id, candidate, 17);
        return SIGIL_OK;
    }

    /* rights_id at 0x230: first 8 bytes BE, no reverse. */
    const uint8_t *rid = decrypted_header + NCA_RIGHTS_ID_OFFSET;
    bool nonzero = false;
    for (int i = 0; i < 16; i++) { if (rid[i] != 0) { nonzero = true; break; } }
    if (nonzero) {
        sigil_hex_encode_8(rid, candidate, false);
        if (candidate[0] == '0' && candidate[1] == '1') {
            memcpy(out_title_id, candidate, 17);
            return SIGIL_OK;
        }
    }

    return SIGIL_ERR_NOT_FOUND;
}

#endif
