// SPDX-License-Identifier: MPL-2.0
#include "sigil_internal.h"
#include <stdlib.h>

#define PS3_SFO_MAX_BYTES (256u * 1024u)

int sigil_extract_ps3(const sigil_io *io, const char *filename_hint,
                      const sigil_options *opts, sigil_result *out) {
    (void)filename_hint;
    (void)opts;

    sigil_result_init(out);
    out->platform     = SIGIL_PLATFORM_PS3;
    out->usage        = SIGIL_USAGE_FOLDER_PREFIX;
    out->experimental = 1;

    if (!io || !io->read || !io->size) return SIGIL_ERR_INVALID_ARG;

    int64_t total = io->size(io->ctx);
    if (total <= 0) return SIGIL_ERR_IO;
    size_t want = (size_t)total;
    if (want > PS3_SFO_MAX_BYTES) want = PS3_SFO_MAX_BYTES;

    uint8_t *buf = (uint8_t *)malloc(want);
    if (!buf) return SIGIL_ERR_OOM;

    int got = io->read(io->ctx, 0, buf, want);
    if (got <= 0) { free(buf); return SIGIL_ERR_IO; }

    char title_id[32] = {0};
    int rc = sigil_sfo_get_string(buf, (size_t)got, "TITLE_ID", title_id, sizeof(title_id));
    free(buf);
    if (rc != SIGIL_OK) return rc;
    if (title_id[0] == '\0') return SIGIL_ERR_NOT_FOUND;

    size_t tid_len = strlen(title_id);
    if (tid_len >= sizeof(out->title_id)) return SIGIL_ERR_NOT_FOUND;

    memcpy(out->title_id, title_id, tid_len + 1);
    memcpy(out->raw_serial, title_id, tid_len + 1);
    out->source = SIGIL_SOURCE_BINARY;
    return SIGIL_OK;
}
