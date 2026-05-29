#ifndef CBT_H
#define CBT_H

#include <stddef.h>

int cbt_extract(const char *cbt_path, const char *cache_root,
                char *out_dir, size_t cap);

#endif
