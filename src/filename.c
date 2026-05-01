/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"
#include <ctype.h>
#include <string.h>

/* M2 stub — full per-platform regex-free scanners land in M2.
 * For now we expose the entry point so sigil.c can call into it; it just
 * returns NOT_FOUND, which lets the rest of the dispatch chain work. */
int sigil_filename_fallback(const char *filename_hint,
                            sigil_platform platform,
                            sigil_result *out) {
    (void)filename_hint;
    (void)platform;
    (void)out;
    return SIGIL_ERR_NOT_FOUND;
}
