#ifndef ARCHIVE_UTIL_H
#define ARCHIVE_UTIL_H

#include <stddef.h>

void        au_hash_hex(const char *s, char out[17]);
int         au_has_img_ext(const char *name);
int         au_safe_name(const char *name);
int         au_mkpath(const char *path);
int         au_dir_has_done(const char *dir);
void        au_mark_done(const char *dir);
const char *au_basename(const char *path);

#endif
