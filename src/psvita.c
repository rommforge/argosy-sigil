/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

/* PS Vita is filename-only today: no PKG/VPK binary parser exists in
 * argosy's Kotlin code, so sigil mirrors the same scope. The actual ID
 * extraction lives in src/filename.c; this shim just delegates and marks
 * the source accordingly. */
int sigil_extract_psvita(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out) {
    (void)io;
    (void)opts;
    return sigil_filename_fallback(filename_hint, SIGIL_PLATFORM_PSVITA, out);
}
