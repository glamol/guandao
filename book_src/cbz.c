#include "cbz.h"
#include "miniz.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static void hash_hex(const char *s, char out[17]) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 0x100000001b3ULL;
    }
    snprintf(out, 17, "%016llx", (unsigned long long)h);
}

static int has_img_ext(const char *name) {
    const char *d = strrchr(name, '.');
    if (!d) return 0;
    return strcasecmp(d, ".png") == 0 || strcasecmp(d, ".jpg") == 0 ||
           strcasecmp(d, ".jpeg") == 0 || strcasecmp(d, ".webp") == 0;
}

static int safe_name(const char *name) {
    if (!name || !*name) return 0;
    if (strstr(name, "..")) return 0;
    if (name[0] == '/') return 0;
    return 1;
}

static int mkpath(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%s", path);
    size_t n = strlen(tmp);
    if (n && tmp[n - 1] == '/') tmp[--n] = 0;
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int dir_has_files(const char *dir) {
    char probe[1024];
    snprintf(probe, sizeof probe, "%s", dir);
    struct stat st;
    if (stat(probe, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    /* simple heuristic: directory exists and is non-empty enough.
       full check is overkill here — opendir would do it but POSIX only. */
    FILE *marker = NULL;
    snprintf(probe, sizeof probe, "%s/.done", dir);
    marker = fopen(probe, "r");
    if (marker) { fclose(marker); return 1; }
    return 0;
}

static void mark_done(const char *dir) {
    char p[1024];
    snprintf(p, sizeof p, "%s/.done", dir);
    FILE *f = fopen(p, "w");
    if (f) fclose(f);
}

static const char *basename_of(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

int cbz_extract(const char *cbz_path, const char *cache_root,
                char *out_dir, size_t cap) {
    char hash[17];
    hash_hex(cbz_path, hash);
    snprintf(out_dir, cap, "%s/cbz/%s", cache_root, hash);

    if (dir_has_files(out_dir)) return 0;
    if (mkpath(out_dir) != 0) return -1;

    mz_zip_archive zip = {0};
    if (!mz_zip_reader_init_file(&zip, cbz_path, 0)) {
        fprintf(stderr, "cbz: failed to open %s\n", cbz_path);
        return -1;
    }

    mz_uint count = mz_zip_reader_get_num_files(&zip);
    int extracted = 0;
    for (mz_uint i = 0; i < count; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        if (st.m_is_directory) continue;
        if (!safe_name(st.m_filename)) continue;
        if (!has_img_ext(st.m_filename)) continue;

        const char *leaf = basename_of(st.m_filename);
        char out_path[1024];
        snprintf(out_path, sizeof out_path, "%s/%s", out_dir, leaf);

        if (!mz_zip_reader_extract_to_file(&zip, i, out_path, 0)) {
            fprintf(stderr, "cbz: extract failed: %s\n", st.m_filename);
            continue;
        }
        extracted++;
    }
    mz_zip_reader_end(&zip);

    if (extracted == 0) return -1;
    mark_done(out_dir);
    return 0;
}
