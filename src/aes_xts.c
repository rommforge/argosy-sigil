/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

#if SIGIL_WITH_SWITCH

#include "aes.h"   /* tiny-AES-c */
#include <string.h>

/* Nintendo-variant AES-XTS notes (from argosy's AesXts.kt:74-83 and the
 * commit message of e6997dd8 "Fix AES-XTS to use big-endian sector
 * numbers"):
 *
 *   The tweak INPUT is built as a 16-byte buffer with bytes 0-7 zero and
 *   bytes 8-15 holding the sector number in big-endian order (sector_num
 *   & 0xFF in byte 15, the next byte in 14, ...).
 *
 *   Standard IEEE P1619 puts the sector number little-endian in bytes 0-7
 *   with bytes 8-15 zero. Test vectors generated against generic XTS will
 *   pass at sector 0 (zero is zero in either encoding) but fail at any
 *   sector >= 1. This is a documented prior bug — argosy shipped the
 *   standard variant first, then fixed it. Ports MUST follow the Nintendo
 *   form.
 *
 *   The GF(2^128) multiplication (`mul_alpha`) below IS standard: shift
 *   left by 1 with byte 0 the LSB and byte 15 the MSB, XOR 0x87 into byte
 *   0 if the high bit was set. Only the tweak input is non-standard.
 */

static inline void mul_alpha(uint8_t t[16]) {
    uint8_t carry = (t[15] >> 7) & 1;
    for (int i = 15; i >= 1; i--) {
        t[i] = (uint8_t)((t[i] << 1) | (t[i - 1] >> 7));
    }
    t[0] = (uint8_t)(t[0] << 1);
    if (carry) t[0] ^= 0x87;
}

void sigil_aes_xts_decrypt_nintendo(const uint8_t key[32],
                                     uint64_t start_sector,
                                     uint8_t *data, size_t len) {
    struct AES_ctx data_ctx;
    struct AES_ctx tweak_ctx;
    AES_init_ctx(&data_ctx,  key);        /* key1 = first 16 bytes */
    AES_init_ctx(&tweak_ctx, key + 16);   /* key2 = next 16 bytes */

    const size_t SECTOR = 0x200;
    size_t full_sectors = len / SECTOR;

    for (size_t s = 0; s < full_sectors; s++) {
        /* Build Nintendo-variant tweak input. */
        uint8_t tweak[16] = {0};
        uint64_t n = start_sector + s;
        for (int i = 15; i >= 8; i--) {
            tweak[i] = (uint8_t)(n & 0xFF);
            n >>= 8;
        }
        AES_ECB_encrypt(&tweak_ctx, tweak);

        uint8_t *sector_base = data + s * SECTOR;
        for (size_t off = 0; off + 16 <= SECTOR; off += 16) {
            uint8_t *block = sector_base + off;
            for (int i = 0; i < 16; i++) block[i] ^= tweak[i];
            AES_ECB_decrypt(&data_ctx, block);
            for (int i = 0; i < 16; i++) block[i] ^= tweak[i];
            mul_alpha(tweak);
        }
    }

    /* Handle a trailing partial sector (the NCA header path always uses
     * exact 0xC00 = 6 full sectors so this branch is rarely exercised, but
     * preserve the original Kotlin behavior for robustness). */
    size_t remaining_off = full_sectors * SECTOR;
    if (remaining_off < len) {
        uint8_t tweak[16] = {0};
        uint64_t n = start_sector + full_sectors;
        for (int i = 15; i >= 8; i--) {
            tweak[i] = (uint8_t)(n & 0xFF);
            n >>= 8;
        }
        AES_ECB_encrypt(&tweak_ctx, tweak);

        size_t off = remaining_off;
        while (off + 16 <= len) {
            uint8_t *block = data + off;
            for (int i = 0; i < 16; i++) block[i] ^= tweak[i];
            AES_ECB_decrypt(&data_ctx, block);
            for (int i = 0; i < 16; i++) block[i] ^= tweak[i];
            mul_alpha(tweak);
            off += 16;
        }
    }
}

#endif /* SIGIL_WITH_SWITCH */
