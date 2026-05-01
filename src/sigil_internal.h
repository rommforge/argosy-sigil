/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SIGIL_INTERNAL_H
#define SIGIL_INTERNAL_H

#include "sigil.h"
#include <stdbool.h>
#include <string.h>

/* ---- Byte-order helpers ------------------------------------------------- */

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

/* ---- I/O convenience ---------------------------------------------------- */

/* Reads exactly `len` bytes; returns SIGIL_OK on full read, error code
 * otherwise. Never returns short. */
int sigil_io_read_exact(const sigil_io *io, uint64_t off, void *buf, size_t len);

/* ---- ISO9660 walker (sector callback) ----------------------------------- */

#define SIGIL_ISO_SECTOR_SIZE 2048

typedef struct {
    uint32_t lba;
    uint32_t length;
} sigil_iso_file_loc;

/* Read sector `lba` (2048 bytes) into `out`. Returns SIGIL_OK on success. */
int sigil_iso_read_sector(const sigil_io *io, uint32_t lba, uint8_t out[SIGIL_ISO_SECTOR_SIZE]);

/* Read PVD at sector 16, validate "CD001", extract root directory LBA/length.
 * Returns SIGIL_OK on success, SIGIL_ERR_NOT_FOUND if PVD invalid. */
int sigil_iso_read_pvd_root(const sigil_io *io, uint32_t *root_lba, uint32_t *root_len);

/* Find a file in the directory at (dir_lba, dir_len). Case-insensitive name
 * match, accepts both "NAME" and "NAME;1" variants. Returns SIGIL_OK and
 * fills `out` on hit; SIGIL_ERR_NOT_FOUND otherwise. */
int sigil_iso_find_file(const sigil_io *io,
                        uint32_t dir_lba, uint32_t dir_len,
                        const char *target_name,
                        sigil_iso_file_loc *out);

/* ---- SYSTEM.CNF / boot-line parser (PSX + PS2) -------------------------- */

/* Parse a boot line from SYSTEM.CNF text. Looks for `boot_key` (e.g. "BOOT2"
 * or "BOOT") followed by `=`, optional whitespace, optional cdrom prefix
 * (`cdrom0:` or `cdrom:` with optional `\`), then the LLLL[_.]NNN.NN serial
 * terminated by `;`.
 *
 * Writes:
 *   raw[0..10]       e.g. "SLUS_201.05" — preserves the original separators
 *   canonical[0..10] e.g. "SLUS-20105"  — dashed RomM form
 *
 * Both buffers must hold at least 11 bytes. Returns SIGIL_OK on match. */
int sigil_cnf_parse_boot(const uint8_t *cnf, size_t len,
                         const char *boot_key,
                         char raw[32], char canonical[32]);

/* ---- Per-platform extractors -------------------------------------------- */

int sigil_extract_psp(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_psx(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out);
int sigil_extract_ps2(const sigil_io *io, const char *filename_hint,
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

/* ---- Filename fallback -------------------------------------------------- */

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

/* ---- Result init -------------------------------------------------------- */

static inline void sigil_result_init(sigil_result *r) {
    memset(r, 0, sizeof(*r));
    r->struct_version = SIGIL_RESULT_V1;
}

#endif /* SIGIL_INTERNAL_H */
