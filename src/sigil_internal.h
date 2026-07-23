// SPDX-License-Identifier: MPL-2.0
#ifndef SIGIL_INTERNAL_H
#define SIGIL_INTERNAL_H

#include "sigil.h"
#include "sigil_util.h"
#include <stdbool.h>
#include <string.h>

#define SIGIL_NCA_HEADER_SIZE 0xC00

static inline uint32_t sigil_read_le32(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static inline uint64_t sigil_read_le64(const uint8_t *p) {
    return (uint64_t)sigil_read_le32(p)
         | ((uint64_t)sigil_read_le32(p + 4) << 32);
}

static inline uint32_t sigil_read_be32(const uint8_t *p) {
    return (uint32_t)p[3]
         | ((uint32_t)p[2] << 8)
         | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[0] << 24);
}

static inline uint64_t sigil_read_be64(const uint8_t *p) {
    return ((uint64_t)sigil_read_be32(p) << 32)
         |  (uint64_t)sigil_read_be32(p + 4);
}

int sigil_io_read_exact(const sigil_io *io, uint64_t off, void *buf, size_t len);

#define SIGIL_ISO_SECTOR_SIZE 2048

typedef struct {
    uint32_t lba;
    uint32_t length;
} sigil_iso_file_loc;

int sigil_iso_read_sector(const sigil_io *io, uint32_t lba, uint8_t out[SIGIL_ISO_SECTOR_SIZE]);
int sigil_iso_read_pvd_root(const sigil_io *io, uint32_t *root_lba, uint32_t *root_len);
int sigil_iso_find_file(const sigil_io *io,
                        uint32_t dir_lba, uint32_t dir_len,
                        const char *target_name,
                        sigil_iso_file_loc *out);

#if SIGIL_WITH_SWITCH
void sigil_aes_xts_decrypt_nintendo(const uint8_t key[32],
                                     uint64_t start_sector,
                                     uint8_t *data, size_t len);

/* Inverse of the above. Not used by extraction; exists so tests can build
 * encrypted NCA fixtures deterministically. */
void sigil_aes_xts_encrypt_nintendo(const uint8_t key[32],
                                     uint64_t start_sector,
                                     uint8_t *data, size_t len);

int sigil_decode_header_key_from_text(const char *text, size_t text_len,
                                       uint8_t out[32]);

int sigil_resolve_header_key(const sigil_support *sup, uint8_t out[32]);

int sigil_nca_extract_title_id(const uint8_t *decrypted_header, char out_title_id[17]);

/* Tries the header as plaintext first, then decrypts a copy if a key is
 * present. SIGIL_ERR_NEEDS_KEY when plaintext fails and no key is given. */
int sigil_nca_title_from_raw_header(const uint8_t *raw_header,
                                    const uint8_t *header_key_or_null,
                                    char out_title_id[17]);
#endif

int sigil_cnf_parse_boot(const uint8_t *cnf, size_t len,
                         const char *boot_key,
                         char raw[32], char canonical[32]);

int sigil_sfo_get_string(const uint8_t *data, size_t len,
                         const char *key,
                         char *out, size_t out_cap);

int sigil_extract_psp(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_psx(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_ps2(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_ps3(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_xbox360(const sigil_io *io, const char *filename_hint,
                          const sigil_options *opts, sigil_result *out);
int sigil_extract_wii(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_gamecube(const sigil_io *io, const char *filename_hint,
                           const sigil_options *opts, sigil_result *out);
int sigil_extract_3ds(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_switch(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out);
int sigil_extract_wiiu(const sigil_io *io, const char *filename_hint,
                       const sigil_options *opts, sigil_result *out);
int sigil_extract_psvita(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out);

#if SIGIL_WITH_FILENAME
int sigil_filename_fallback(const char *filename_hint,
                            sigil_platform platform,
                            sigil_result *out);
#else
static inline int sigil_filename_fallback(const char *filename_hint,
                                           sigil_platform platform,
                                           sigil_result *out) {
    (void)filename_hint; (void)platform; (void)out;
    return SIGIL_ERR_NOT_FOUND;
}
#endif

static inline void sigil_result_init(sigil_result *r) {
    memset(r, 0, sizeof(*r));
    r->struct_version = SIGIL_RESULT_V1;
}

#endif
