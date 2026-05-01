/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SIGIL_H
#define SIGIL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Versioned struct identifiers --------------------------------------- */

#define SIGIL_RESULT_V1   1u
#define SIGIL_SUPPORT_V1  1u
#define SIGIL_OPTIONS_V1  1u

/* ---- Error codes -------------------------------------------------------- */

#define SIGIL_OK                       0
#define SIGIL_ERR_INVALID_ARG         -1
#define SIGIL_ERR_IO                  -2
#define SIGIL_ERR_UNKNOWN_PLATFORM    -3
#define SIGIL_ERR_UNSUPPORTED_FORMAT  -4
#define SIGIL_ERR_NOT_FOUND           -5
#define SIGIL_ERR_NEEDS_KEY           -6
#define SIGIL_ERR_CRYPTO              -7
#define SIGIL_ERR_OOM                 -8

/* ---- Option flags ------------------------------------------------------- */

#define SIGIL_FLAG_FILENAME_FALLBACK   (1u << 0)
#define SIGIL_FLAG_3DS_ALLOW_HOMEBREW  (1u << 1)

/* ---- Enums -------------------------------------------------------------- */

typedef enum {
    SIGIL_PLATFORM_AUTO = 0,
    SIGIL_PLATFORM_PSP,
    SIGIL_PLATFORM_PSX,
    SIGIL_PLATFORM_PS2,
    SIGIL_PLATFORM_PSVITA,
    SIGIL_PLATFORM_SWITCH,
    SIGIL_PLATFORM_3DS,
    SIGIL_PLATFORM_WII,
    SIGIL_PLATFORM_WIIU,
    SIGIL_PLATFORM_GAMECUBE
} sigil_platform;

typedef enum {
    SIGIL_SOURCE_BINARY = 0,
    SIGIL_SOURCE_FILENAME = 1
} sigil_source;

/* How the platform uses the title ID to name save artifacts on disk.
 * EXACT vs PREFIX is load-bearing: PREFIX platforms have multiple save
 * artifacts per game (different profiles, save slots, system data) that
 * consumers MUST match-all-and-bundle, not assume a single hit. */
typedef enum {
    SIGIL_USAGE_FOLDER_EXACT = 0,
    SIGIL_USAGE_FOLDER_PREFIX = 1,
    SIGIL_USAGE_FILE_EXACT = 2,
    SIGIL_USAGE_FILE_PREFIX = 3
} sigil_usage;

/* ---- Result ------------------------------------------------------------- */

typedef struct {
    uint32_t       struct_version;     /* SIGIL_RESULT_V1 */
    char           title_id[32];       /* canonical save-matching form, NUL-terminated */
    char           raw_serial[32];     /* as it appears in the binary, NUL-terminated */
    sigil_platform platform;
    sigil_source   source;
    sigil_usage    usage;
} sigil_result;

/* ---- I/O abstraction ---------------------------------------------------- */

typedef struct sigil_io sigil_io;

struct sigil_io {
    /* Read `len` bytes at `off` into `buf`. Returns bytes actually read,
     * or a negative value on error. May return less than `len` near EOF. */
    int     (*read)(void *ctx, uint64_t off, void *buf, size_t len);
    /* Total stream size in bytes, or -1 if unknown. */
    int64_t (*size)(void *ctx);
    /* Free any per-stream state. May be NULL. */
    void    (*close)(void *ctx);
    void     *ctx;
};

/* ---- Support context (per-platform external resources) ------------------ */

/* All fields optional. NULL/zero = best-effort without that platform's
 * external resources. The library never fails just because support context
 * is missing; it falls back to whatever extraction is possible. */
typedef struct {
    uint32_t       struct_version;     /* SIGIL_SUPPORT_V1 */

    /* Switch: prod.keys content for AES-XTS NCA header decryption.
     * Provide raw key OR text path OR text blob; raw key wins if both set.
     * Without any of these, sigil tries the unencrypted Switch path
     * (filename-table .nca name match) and returns SIGIL_SOURCE_FILENAME
     * on encrypted-retail dumps. */
    const uint8_t *switch_header_key;       /* 32 bytes, or NULL */
    const char    *switch_prod_keys_path;   /* path to prod.keys, or NULL */
    const char    *switch_prod_keys_text;   /* in-memory blob, or NULL */
    size_t         switch_prod_keys_text_len;
} sigil_support;

/* ---- Options ------------------------------------------------------------ */

typedef struct {
    uint32_t              struct_version;  /* SIGIL_OPTIONS_V1 */
    const sigil_support  *support;         /* may be NULL */
    uint32_t              flags;           /* SIGIL_FLAG_* */
} sigil_options;

/* ---- Extraction --------------------------------------------------------- */

/* Extract the title ID from a file path. Sigil opens the file, sniffs the
 * platform if `hint` is SIGIL_PLATFORM_AUTO, and dispatches to the right
 * extractor. Returns SIGIL_OK and fills `out` on success.
 *
 * `opts` may be NULL for defaults (no support context, filename fallback
 * enabled). */
int sigil_extract_from_path(const char *path,
                            sigil_platform hint,
                            const sigil_options *opts,
                            sigil_result *out);

/* Same as above but reads through a caller-supplied I/O abstraction.
 * `filename_hint` is used for extension sniffing and filename fallback;
 * may be NULL. */
int sigil_extract_from_io(const sigil_io *io,
                          const char *filename_hint,
                          sigil_platform hint,
                          const sigil_options *opts,
                          sigil_result *out);

/* ---- Platform slug helpers ---------------------------------------------- */

/* Returns SIGIL_PLATFORM_AUTO if `slug` is NULL or unknown. */
sigil_platform sigil_platform_from_slug(const char *slug);

/* Returns "auto" for invalid values. Pointer is to a static string; do not free. */
const char *sigil_platform_to_slug(sigil_platform p);

/* ---- I/O factories ------------------------------------------------------ */

/* Open a regular file as a sigil_io. Returns NULL on error.
 * Free with sigil_io_close. */
sigil_io *sigil_io_open_file(const char *path);

/* Open a CHD file as a sigil_io that yields cooked 2048-byte ISO9660 sectors.
 * Requires SIGIL_WITH_CHD. Returns NULL on error or if CHD support was
 * disabled at build time. Free with sigil_io_close. */
sigil_io *sigil_io_open_chd(const char *path);

/* Open a raw `.bin` CD image (typically 2352-byte sectors) as a sigil_io
 * yielding cooked 2048-byte sectors. Detects MODE1/MODE2 from the CD sync
 * pattern at sector 16. Returns NULL on error. Free with sigil_io_close. */
sigil_io *sigil_io_open_raw_cd(const char *path);

void sigil_io_close(sigil_io *io);

/* ---- Switch key helpers ------------------------------------------------- */

/* Read `header_key` from a prod.keys text file. Writes 32 bytes to `out`.
 * Returns SIGIL_OK on success or a negative error code. */
int sigil_load_header_key_from_prod_keys(const char *path, uint8_t out[32]);

/* ---- Misc --------------------------------------------------------------- */

const char *sigil_strerror(int code);
const char *sigil_version(void);

#ifdef __cplusplus
}
#endif

#endif /* SIGIL_H */
