#include "cbz.h"
#include "archive_util.h"
#include "miniz.h"

#include <stdio.h>

int cbz_extract(const char *cbz_path, const char *cache_root,
                char *out_dir, size_t cap) {
    char hash[17];
    au_hash_hex(cbz_path, hash);
    snprintf(out_dir, cap, "%s/cbz/%s", cache_root, hash);

    if (au_dir_has_done(out_dir)) return 0;
    if (au_mkpath(out_dir) != 0) return -1;

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
        if (!au_safe_name(st.m_filename)) continue;
        if (!au_has_img_ext(st.m_filename)) continue;

        const char *leaf = au_basename(st.m_filename);
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
    au_mark_done(out_dir);
    return 0;
}
