/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "integration_helpers.h"

static int check(const char *path, const char *name) {
    sigil_result r;
    int rc = sigil_extract_from_path(path, SIGIL_PLATFORM_PSVITA, NULL, &r);
    if (rc != SIGIL_OK) {
        fprintf(stderr, "  FAIL %s: %s\n", name, sigil_strerror(rc));
        return -1;
    }
    /* Vita canonical: 9 chars LLLL\d{5}; FOLDER_EXACT. Filename-only today. */
    if (strlen(r.title_id) != 9 || r.usage != SIGIL_USAGE_FOLDER_EXACT) {
        fprintf(stderr, "  FAIL %s: title_id=%s usage=%d\n", name, r.title_id, r.usage);
        return -1;
    }
    fprintf(stdout, "  ok  %s -> %s (raw=%s, src=%s)\n", name, r.title_id, r.raw_serial,
            r.source == SIGIL_SOURCE_BINARY ? "binary" : "filename");
    return 0;
}

int main(void) {
    const char *rom_dir = get_rom_dir();
    if (!rom_dir) { fprintf(stderr, "SIGIL_ROM_DIR not set; skipping\n"); return TEST_SKIP; }
    char path[512];
    if (build_subdir(rom_dir, "psvita", path) != 0) return 1;
    const char *exts[] = { "zip", NULL };
    walk_stats st = walk_dir(path, check, exts);
    fprintf(stdout, "PS Vita: processed=%d passed=%d failed=%d\n",
            st.processed, st.passed, st.failed);
    if (st.processed == 0) { fprintf(stderr, "no Vita samples\n"); return TEST_SKIP; }
    int threshold = (st.processed * 4) / 5;
    return st.passed >= threshold ? 0 : 1;
}
