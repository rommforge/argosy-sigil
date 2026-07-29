// SPDX-License-Identifier: MPL-2.0
#include "integration_helpers.h"

static int check(const char *path, const char *name) {
    sigil_result r;
    int rc = sigil_extract_from_path(path, SIGIL_PLATFORM_3DS, NULL, &r);
    if (rc != SIGIL_OK) {
        fprintf(stderr, "  FAIL %s: %s\n", name, sigil_strerror(rc));
        return -1;
    }
    /* 3DS retail: 16 hex chars starting with "0004". */
    if (strlen(r.title_id) != 16) {
        fprintf(stderr, "  FAIL %s: title_id=%s (want 16 hex)\n", name, r.title_id);
        return -1;
    }
    if (memcmp(r.title_id, "0004", 4) != 0) {
        fprintf(stderr, "  FAIL %s: title_id=%s (want 0004 prefix)\n", name, r.title_id);
        return -1;
    }
    if (r.source != SIGIL_SOURCE_BINARY) {
        fprintf(stderr, "  FAIL %s: source=filename\n", name);
        return -1;
    }
    if (r.usage != SIGIL_USAGE_FOLDER_SPLIT) {
        fprintf(stderr, "  FAIL %s: usage=%d (want folder-split)\n", name, r.usage);
        return -1;
    }
    /* save_id is the on-disk split, not the flat id: <high 8>/<low 8>. */
    char want_save_id[18];
    snprintf(want_save_id, sizeof(want_save_id), "%.8s/%.8s", r.title_id, r.title_id + 8);
    if (strcmp(r.save_id, want_save_id) != 0) {
        fprintf(stderr, "  FAIL %s: save_id=%s (want %s)\n", name, r.save_id, want_save_id);
        return -1;
    }
    fprintf(stdout, "  ok  %s -> %s\n", name, r.save_id);
    return 0;
}

int main(void) {
    const char *rom_dir = get_rom_dir();
    if (!rom_dir) { fprintf(stderr, "SIGIL_ROM_DIR not set; skipping\n"); return TEST_SKIP; }
    char path[512];
    if (build_subdir(rom_dir, "3ds", path) != 0) return 1;
    const char *exts[] = { "3ds", "cci", "z3ds", "zcci", NULL };
    walk_stats st = walk_dir(path, check, exts);
    fprintf(stdout, "3DS: processed=%d passed=%d failed=%d\n",
            st.processed, st.passed, st.failed);
    if (st.processed == 0) { fprintf(stderr, "no 3DS samples\n"); return TEST_SKIP; }
    int threshold = (st.processed * 4) / 5;
    return st.passed >= threshold ? 0 : 1;
}
