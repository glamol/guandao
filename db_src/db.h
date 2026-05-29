#ifndef GUANDAO_DB_H
#define GUANDAO_DB_H

#include <stddef.h>
#include <stdint.h>
#include <sqlite3.h>

typedef struct {
    sqlite3 *handle;
} Db;

int  db_open(Db *db, const char *path);
void db_close(Db *db);

/* library */
int  db_library_upsert(Db *db, const char *path, const char *kind,
                       const char *title, int64_t *out_id);
int  db_library_set_progress(Db *db, int64_t id, int page);
int  db_library_get_progress(Db *db, const char *path); /* -1 if absent */
int  db_library_list(Db *db, sqlite3_stmt **stmt);

/* pages */
int  db_page_upsert(Db *db, int64_t lib_id, int page_no,
                    const char *image_path, int64_t *out_id);

/* blobs */
int  db_blob_add(Db *db, int64_t page_id, int idx,
                 const char *bbox, const char *text, int64_t *out_id);
int  db_blobs_for_page(Db *db, int64_t page_id, sqlite3_stmt **stmt);

/* translations: cache keyed by content hash */
int  db_translation_get(Db *db, const char *text_hash, const char *lang,
                        char *out, size_t out_sz); /* SQLITE_OK on hit, SQLITE_NOTFOUND on miss */
int  db_translation_put(Db *db, const char *text_hash, const char *lang,
                        const char *engine, const char *translated);

/* fnv1a64 hex of text, writes 17 bytes to out (16 hex + NUL) */
void db_text_hash(const char *text, char out[17]);

#endif
