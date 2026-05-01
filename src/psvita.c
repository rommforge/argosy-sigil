// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"

/* PS Vita is filename-only — no PKG/VPK binary parser. */
int sigil_extract_psvita(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out) {
    (void)io;
    (void)opts;
    return sigil_filename_fallback(filename_hint, SIGIL_PLATFORM_PSVITA, out);
}
