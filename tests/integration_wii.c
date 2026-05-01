/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "integration_helpers.h"

static int check(const char *path, const char *name) {
    sigil_result r;
    int rc = sigil_extract_from_path(path, SIGIL_PLATFORM_WII, NULL, &r);
    if (rc != SIGIL_OK) {
        fprintf(stderr, "  FAIL %s: %s\n", name, sigil_strerror(rc));
        return -1;
    }
    /* Wii canonical: 8 hex chars (hex of 4 ASCII gameId bytes). */
    if (strlen(r.title_id) != 8 || strlen(r.raw_serial) != 4) {
        fprintf(stderr, "  FAIL %s: title_id=%s raw=%s\n",
                name, r.title_id, r.raw_serial);
        return -1;
    }
    fprintf(stdout, "  ok  %s -> %s (raw=%s)\n", name, r.title_id, r.raw_serial);
    return 0;
}

int main(void) {
    const char *rom_dir = get_rom_dir();
    if (!rom_dir) { fprintf(stderr, "SIGIL_ROM_DIR not set; skipping\n"); return TEST_SKIP; }
    char path[512];
    if (build_subdir(rom_dir, "wii", path) != 0) return 1;
    const char *exts[] = { "iso", "rvz", NULL };
    walk_stats st = walk_dir(path, check, exts);
    fprintf(stdout, "Wii: processed=%d passed=%d failed=%d\n",
            st.processed, st.passed, st.failed);
    if (st.processed == 0) { fprintf(stderr, "no Wii samples\n"); return TEST_SKIP; }
    return st.failed ? 1 : 0;
}
