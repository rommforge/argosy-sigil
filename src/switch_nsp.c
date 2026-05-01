/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

/* M3 stub. */
int sigil_extract_switch(const sigil_io *io, const char *filename_hint,
                         const sigil_options *opts, sigil_result *out) {
    (void)io; (void)filename_hint; (void)opts; (void)out;
    return SIGIL_ERR_UNSUPPORTED_FORMAT;
}
