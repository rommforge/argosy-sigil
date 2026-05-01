/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "sigil_internal.h"

/* M3 stub. */
int sigil_load_header_key_from_prod_keys(const char *path, uint8_t out[32]) {
    (void)path; (void)out;
    return SIGIL_ERR_UNSUPPORTED_FORMAT;
}
