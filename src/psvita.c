/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

/* PS Vita is filename-only today: no PKG/VPK binary parser. The real
 * extraction happens in src/filename.c (M2). This shim returns NOT_FOUND so
 * the dispatcher can fall through cleanly. */
int sigil_extract_psvita(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out) {
    (void)io; (void)filename_hint; (void)opts; (void)out;
    return SIGIL_ERR_NOT_FOUND;
}
