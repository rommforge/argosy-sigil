// SPDX-License-Identifier: MPL-2.0
#include "integration_helpers.h"

static int check(const char *path, const char *name) {
    sigil_result r;
    int rc = sigil_extract_from_path(path, SIGIL_PLATFORM_PSP, NULL, &r);
    if (rc != SIGIL_OK) {
        fprintf(stderr, "  FAIL %s: %s\n", name, sigil_strerror(rc));
        return -1;
    }
    /* PSP canonical: 9 chars, [A-Z]{4}\d{5}; FOLDER_PREFIX. */
    size_t L = strlen(r.title_id);
    if (L != 9) {
        fprintf(stderr, "  FAIL %s: title_id len=%zu (want 9): %s\n", name, L, r.title_id);
        return -1;
    }
    if (r.usage != SIGIL_USAGE_FOLDER_PREFIX) {
        fprintf(stderr, "  FAIL %s: usage=%d (want FOLDER_PREFIX)\n", name, r.usage);
        return -1;
    }
    if (r.source != SIGIL_SOURCE_BINARY) {
        fprintf(stderr, "  FAIL %s: source=filename (want binary)\n", name);
        return -1;
    }
    fprintf(stdout, "  ok  %s -> %s (raw=%s)\n", name, r.title_id, r.raw_serial);
    return 0;
}

int main(void) {
    const char *rom_dir = get_rom_dir();
    if (!rom_dir) {
        fprintf(stderr, "SIGIL_ROM_DIR not set; skipping\n");
        return TEST_SKIP;
    }
    char path[512];
    if (build_subdir(rom_dir, "psp", path) != 0) return 1;

    const char *exts[] = { "iso", "chd", NULL };
    walk_stats st = walk_dir(path, check, exts);
    fprintf(stdout, "PSP: processed=%d passed=%d failed=%d\n",
            st.processed, st.passed, st.failed);
    if (st.processed == 0) {
        fprintf(stderr, "no PSP samples in %s; skipping\n", path);
        return TEST_SKIP;
    }
    return st.failed ? 1 : 0;
}
