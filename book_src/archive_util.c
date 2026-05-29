#include "archive_util.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

void au_hash_hex(const char *s, char out[17]) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 0x100000001b3ULL;
    }
    snprintf(out, 17, "%016llx", (unsigned long long)h);
}

int au_has_img_ext(const char *name) {
    const char *d = strrchr(name, '.');
    if (!d) return 0;
    return strcasecmp(d, ".png") == 0 || strcasecmp(d, ".jpg") == 0 ||
           strcasecmp(d, ".jpeg") == 0 || strcasecmp(d, ".webp") == 0;
}

int au_safe_name(const char *name) {
    if (!name || !*name) return 0;
    if (strstr(name, "..")) return 0;
    if (name[0] == '/') return 0;
    return 1;
}

int au_mkpath(const char *path) {
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

int au_dir_has_done(const char *dir) {
    char p[1024];
    snprintf(p, sizeof p, "%s/.done", dir);
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}

void au_mark_done(const char *dir) {
    char p[1024];
    snprintf(p, sizeof p, "%s/.done", dir);
    FILE *f = fopen(p, "w");
    if (f) fclose(f);
}

const char *au_basename(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}
