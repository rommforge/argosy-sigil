/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define SIGIL_VERSION_STRING "0.1.0-dev"

/* ---- Slug table --------------------------------------------------------- */

typedef struct {
    sigil_platform p;
    const char    *slug;
} platform_slug;

static const platform_slug PLATFORM_SLUGS[] = {
    { SIGIL_PLATFORM_AUTO,     "auto"     },
    { SIGIL_PLATFORM_PSP,      "psp"      },
    { SIGIL_PLATFORM_PSX,      "psx"      },
    { SIGIL_PLATFORM_PS2,      "ps2"      },
    { SIGIL_PLATFORM_PSVITA,   "psvita"   },
    { SIGIL_PLATFORM_SWITCH,   "switch"   },
    { SIGIL_PLATFORM_3DS,      "3ds"      },
    { SIGIL_PLATFORM_WII,      "wii"      },
    { SIGIL_PLATFORM_WIIU,     "wiiu"     },
    { SIGIL_PLATFORM_GAMECUBE, "gamecube" },
};
static const size_t PLATFORM_SLUG_COUNT = sizeof(PLATFORM_SLUGS) / sizeof(PLATFORM_SLUGS[0]);

/* Slug aliases — argosy uses "vita" internally as well as "psvita"; "ngc"
 * for GameCube. Map them to canonical sigil platforms. */
typedef struct {
    const char     *alias;
    sigil_platform  p;
} platform_alias;

static const platform_alias PLATFORM_ALIASES[] = {
    { "vita",    SIGIL_PLATFORM_PSVITA   },
    { "ngc",     SIGIL_PLATFORM_GAMECUBE },
    { "gc",      SIGIL_PLATFORM_GAMECUBE },
    { "n3ds",    SIGIL_PLATFORM_3DS      },
    { "nsw",     SIGIL_PLATFORM_SWITCH   },
};
static const size_t PLATFORM_ALIAS_COUNT = sizeof(PLATFORM_ALIASES) / sizeof(PLATFORM_ALIASES[0]);

sigil_platform sigil_platform_from_slug(const char *slug) {
    if (!slug) return SIGIL_PLATFORM_AUTO;
    for (size_t i = 0; i < PLATFORM_SLUG_COUNT; i++) {
        if (strcmp(slug, PLATFORM_SLUGS[i].slug) == 0) return PLATFORM_SLUGS[i].p;
    }
    for (size_t i = 0; i < PLATFORM_ALIAS_COUNT; i++) {
        if (strcmp(slug, PLATFORM_ALIASES[i].alias) == 0) return PLATFORM_ALIASES[i].p;
    }
    return SIGIL_PLATFORM_AUTO;
}

const char *sigil_platform_to_slug(sigil_platform p) {
    for (size_t i = 0; i < PLATFORM_SLUG_COUNT; i++) {
        if (PLATFORM_SLUGS[i].p == p) return PLATFORM_SLUGS[i].slug;
    }
    return "auto";
}

/* ---- Version + errors --------------------------------------------------- */

const char *sigil_version(void) { return SIGIL_VERSION_STRING; }

const char *sigil_strerror(int code) {
    switch (code) {
    case SIGIL_OK:                       return "ok";
    case SIGIL_ERR_INVALID_ARG:          return "invalid argument";
    case SIGIL_ERR_IO:                   return "I/O error";
    case SIGIL_ERR_UNKNOWN_PLATFORM:     return "unknown platform";
    case SIGIL_ERR_UNSUPPORTED_FORMAT:   return "unsupported format";
    case SIGIL_ERR_NOT_FOUND:            return "title id not found";
    case SIGIL_ERR_NEEDS_KEY:            return "decryption key required";
    case SIGIL_ERR_CRYPTO:               return "crypto failure";
    case SIGIL_ERR_OOM:                  return "out of memory";
    default:                             return "unknown error";
    }
}

/* ---- Filename + extension helpers --------------------------------------- */

static const char *path_basename(const char *path) {
    if (!path) return NULL;
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (bslash && (!slash || bslash > slash)) slash = bslash;
#endif
    return slash ? slash + 1 : path;
}

static void lower_ext(const char *name, char out[16]) {
    out[0] = '\0';
    if (!name) return;
    const char *dot = strrchr(name, '.');
    if (!dot) return;
    dot++;
    size_t i = 0;
    while (dot[i] && i < 15) {
        out[i] = (char)tolower((unsigned char)dot[i]);
        i++;
    }
    out[i] = '\0';
}

static sigil_platform sniff_from_extension(const char *filename) {
    char ext[16];
    lower_ext(filename, ext);
    if (ext[0] == '\0') return SIGIL_PLATFORM_AUTO;

    /* Unique-extension fast paths. */
    if (strcmp(ext, "nsp") == 0)   return SIGIL_PLATFORM_SWITCH;
    if (strcmp(ext, "xci") == 0)   return SIGIL_PLATFORM_SWITCH;
    if (strcmp(ext, "wua") == 0)   return SIGIL_PLATFORM_WIIU;
    if (strcmp(ext, "3ds") == 0)   return SIGIL_PLATFORM_3DS;
    if (strcmp(ext, "cci") == 0)   return SIGIL_PLATFORM_3DS;
    if (strcmp(ext, "z3ds") == 0)  return SIGIL_PLATFORM_3DS;
    if (strcmp(ext, "zcci") == 0)  return SIGIL_PLATFORM_3DS;
    if (strcmp(ext, "rvz") == 0) {
        /* Could be Wii or GameCube — caller hint disambiguates. The header
         * itself doesn't distinguish; rely on file path / hint. */
        return SIGIL_PLATFORM_WII;
    }
    /* `.iso`, `.bin`, `.chd` are ambiguous between PSP/PSX/PS2/Wii/GC.
     * Without a hint, refuse to guess. */
    return SIGIL_PLATFORM_AUTO;
}

/* ---- Per-platform default IO factory ------------------------------------ */

/* For platforms that can take a `.chd` or raw `.bin` input, we open the
 * appropriate adapter; otherwise plain file IO. */
static sigil_io *open_io_for_platform(const char *path, sigil_platform p) {
    if (!path) return NULL;
    char ext[16];
    lower_ext(path_basename(path), ext);

    bool can_chd = (p == SIGIL_PLATFORM_PSP || p == SIGIL_PLATFORM_PSX
                    || p == SIGIL_PLATFORM_PS2);

#if SIGIL_WITH_CHD
    if (can_chd && strcmp(ext, "chd") == 0) {
        sigil_io *io = sigil_io_open_chd(path);
        if (io) return io;
    }
#else
    (void)can_chd;
#endif
    /* PSX `.bin` raw CD path (cooked-fallback handled by io_raw_cd). */
    if (p == SIGIL_PLATFORM_PSX && strcmp(ext, "bin") == 0) {
        sigil_io *io = sigil_io_open_raw_cd(path);
        if (io) return io;
    }
    return sigil_io_open_file(path);
}

/* ---- Dispatch ----------------------------------------------------------- */

static int dispatch(const sigil_io *io, const char *filename_hint,
                    sigil_platform p, const sigil_options *opts,
                    sigil_result *out) {
    switch (p) {
    case SIGIL_PLATFORM_PSP:      return sigil_extract_psp(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_PSX:      return sigil_extract_psx(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_PS2:      return sigil_extract_ps2(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_WII:      return sigil_extract_wii(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_GAMECUBE: return sigil_extract_gamecube(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_3DS:      return sigil_extract_3ds(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_SWITCH:   return sigil_extract_switch(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_WIIU:     return sigil_extract_wiiu(io, filename_hint, opts, out);
    case SIGIL_PLATFORM_PSVITA:   return sigil_extract_psvita(io, filename_hint, opts, out);
    default:                       return SIGIL_ERR_UNKNOWN_PLATFORM;
    }
}

int sigil_extract_from_io(const sigil_io *io,
                          const char *filename_hint,
                          sigil_platform hint,
                          const sigil_options *opts,
                          sigil_result *out) {
    if (!io || !io->read || !out) return SIGIL_ERR_INVALID_ARG;

    if (hint == SIGIL_PLATFORM_AUTO) {
        hint = sniff_from_extension(filename_hint);
    }
    if (hint == SIGIL_PLATFORM_AUTO) return SIGIL_ERR_UNKNOWN_PLATFORM;

    int rc = dispatch(io, filename_hint, hint, opts, out);

    /* Filename fallback runs on any non-OK result, when enabled. */
    bool fallback_enabled = !opts || (opts->flags & SIGIL_FLAG_FILENAME_FALLBACK)
                            || opts->flags == 0;
    if (rc != SIGIL_OK && fallback_enabled && filename_hint) {
        sigil_result fb;
        int frc = sigil_filename_fallback(filename_hint, hint, &fb);
        if (frc == SIGIL_OK) {
            *out = fb;
            return SIGIL_OK;
        }
    }
    return rc;
}

int sigil_extract_from_path(const char *path, sigil_platform hint,
                            const sigil_options *opts, sigil_result *out) {
    if (!path || !out) return SIGIL_ERR_INVALID_ARG;

    sigil_platform resolved = hint;
    if (resolved == SIGIL_PLATFORM_AUTO) {
        resolved = sniff_from_extension(path_basename(path));
    }

    /* Pure-filename platforms (PS Vita today) need no IO. */
    if (resolved == SIGIL_PLATFORM_PSVITA) {
        return sigil_extract_psvita(NULL, path_basename(path), opts, out);
    }

    if (resolved == SIGIL_PLATFORM_AUTO) {
        /* Try filename fallback only if extension didn't help. */
        if (sigil_filename_fallback(path_basename(path), SIGIL_PLATFORM_AUTO, out) == SIGIL_OK) {
            return SIGIL_OK;
        }
        return SIGIL_ERR_UNKNOWN_PLATFORM;
    }

    sigil_io *io = open_io_for_platform(path, resolved);
    if (!io) return SIGIL_ERR_IO;

    int rc = sigil_extract_from_io(io, path_basename(path), resolved, opts, out);
    sigil_io_close(io);
    return rc;
}
