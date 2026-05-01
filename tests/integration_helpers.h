/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef SIGIL_TEST_HELPERS_H
#define SIGIL_TEST_HELPERS_H

#include "sigil.h"
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TEST_SKIP 77

static const char *get_rom_dir(void) {
    const char *d = getenv("SIGIL_ROM_DIR");
    if (!d || !*d) return NULL;
    return d;
}

/* Build "<rom_dir>/<sub>" into `out`. */
static int build_subdir(const char *rom_dir, const char *sub, char out[512]) {
    int n = snprintf(out, 512, "%s/%s", rom_dir, sub);
    return (n > 0 && n < 512) ? 0 : -1;
}

/* Lower-case extension extractor. */
static void ext_of(const char *name, char out[16]) {
    out[0] = '\0';
    const char *dot = strrchr(name, '.');
    if (!dot) return;
    dot++;
    size_t i = 0;
    while (dot[i] && i < 15) {
        out[i] = (char)((dot[i] >= 'A' && dot[i] <= 'Z') ? dot[i] + 32 : dot[i]);
        i++;
    }
    out[i] = '\0';
}

/* Iterate files in `dir`, call `fn(path, name)` for each. Returns count
 * processed. */
typedef struct {
    int processed;
    int passed;
    int failed;
} walk_stats;

typedef int (*walk_fn)(const char *full_path, const char *basename);

static int sample_limit_from_env(int default_limit) {
    const char *v = getenv("SIGIL_SAMPLE_LIMIT");
    if (!v) return default_limit;
    int n = atoi(v);
    return n > 0 ? n : default_limit;
}

static walk_stats walk_dir(const char *dir, walk_fn fn, const char *filter_exts[]) {
    walk_stats st = {0};
    int limit = sample_limit_from_env(25);
    DIR *dp = opendir(dir);
    if (!dp) return st;

    struct dirent *e;
    while ((e = readdir(dp)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (st.processed >= limit) break;

        char ext[16];
        ext_of(e->d_name, ext);
        if (filter_exts) {
            bool match = false;
            for (int i = 0; filter_exts[i]; i++) {
                if (strcmp(ext, filter_exts[i]) == 0) { match = true; break; }
            }
            if (!match) continue;
        }

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);

        struct stat sb;
        if (stat(full, &sb) != 0 || !S_ISREG(sb.st_mode)) continue;

        st.processed++;
        int rc = fn(full, e->d_name);
        if (rc == 0) st.passed++;
        else         st.failed++;
    }
    closedir(dp);
    return st;
}

#endif
