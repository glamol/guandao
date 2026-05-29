#ifndef CBZ_H
#define CBZ_H

#include <stddef.h>

/* extract `cbz_path` into a stable cache dir.
   writes resulting dir path to `out_dir`. idempotent: if cache
   already populated with images, skips re-extraction.
   returns 0 on success, non-zero on failure. */
int cbz_extract(const char *cbz_path, const char *cache_root,
                char *out_dir, size_t cap);

#endif
