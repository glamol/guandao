#include "cbt.h"
#include "archive_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK 512

static unsigned long parse_octal(const char *s, size_t n) {
    unsigned long v = 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == 0 || c == ' ') break;
        if (c < '0' || c > '7') return 0;
        v = (v << 3) | (unsigned)(c - '0');
    }
    return v;
}

static int is_zero_block(const char *buf) {
    for (int i = 0; i < BLOCK; i++) if (buf[i]) return 0;
    return 1;
}

int cbt_extract(const char *cbt_path, const char *cache_root,
                char *out_dir, size_t cap) {
    char hash[17];
    au_hash_hex(cbt_path, hash);
    snprintf(out_dir, cap, "%s/cbt/%s", cache_root, hash);

    if (au_dir_has_done(out_dir)) return 0;
    if (au_mkpath(out_dir) != 0) return -1;

    FILE *f = fopen(cbt_path, "rb");
    if (!f) {
        fprintf(stderr, "cbt: failed to open %s\n", cbt_path);
        return -1;
    }

    char hdr[BLOCK];
    int extracted = 0;
    int zero_streak = 0;

    while (fread(hdr, 1, BLOCK, f) == BLOCK) {
        if (is_zero_block(hdr)) {
            if (++zero_streak >= 2) break;
            continue;
        }
        zero_streak = 0;

        char name[101];
        memcpy(name, hdr, 100);
        name[100] = 0;

        unsigned long size = parse_octal(hdr + 124, 12);
        char type = hdr[156];

        size_t pad = (size + BLOCK - 1) / BLOCK * BLOCK;

        int is_regular = (type == '0' || type == 0);
        int ok = is_regular && au_safe_name(name) && au_has_img_ext(name);

        if (!ok) {
            if (fseek(f, (long)pad, SEEK_CUR) != 0) break;
            continue;
        }

        const char *leaf = au_basename(name);
        char out_path[1024];
        snprintf(out_path, sizeof out_path, "%s/%s", out_dir, leaf);

        FILE *out = fopen(out_path, "wb");
        if (!out) {
            if (fseek(f, (long)pad, SEEK_CUR) != 0) break;
            continue;
        }

        unsigned long left = size;
        char buf[BLOCK];
        while (left > 0) {
            size_t want = left > BLOCK ? BLOCK : (size_t)left;
            if (fread(buf, 1, BLOCK, f) != BLOCK) { left = 0; break; }
            fwrite(buf, 1, want, out);
            left -= want;
        }
        fclose(out);

        size_t consumed = (size + BLOCK - 1) / BLOCK * BLOCK;
        size_t remaining = pad - consumed;
        if (remaining) fseek(f, (long)remaining, SEEK_CUR);

        extracted++;
    }
    fclose(f);

    if (extracted == 0) return -1;
    au_mark_done(out_dir);
    return 0;
}
