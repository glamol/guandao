#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCHEMA_VERSION 1

static int exec(Db *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db: %s\n", err ? err : sqlite3_errmsg(db->handle));
        sqlite3_free(err);
    }
    return rc;
}

static int migrate(Db *db) {
    if (exec(db, "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER);") != SQLITE_OK)
        return -1;

    sqlite3_stmt *s = NULL;
    int cur = 0;
    if (sqlite3_prepare_v2(db->handle, "SELECT version FROM schema_version;", -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW) cur = sqlite3_column_int(s, 0);
        sqlite3_finalize(s);
    }

    if (cur >= SCHEMA_VERSION) return 0;

    const char *schema =
        "CREATE TABLE IF NOT EXISTS library ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  kind TEXT NOT NULL CHECK(kind IN ('book','manga')),"
        "  path TEXT NOT NULL UNIQUE,"
        "  title TEXT,"
        "  author TEXT,"
        "  source_lang TEXT,"
        "  cover_path TEXT,"
        "  added_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
        "  last_opened_at INTEGER,"
        "  last_page INTEGER NOT NULL DEFAULT 0,"
        "  total_pages INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS pages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  library_id INTEGER NOT NULL REFERENCES library(id) ON DELETE CASCADE,"
        "  page_no INTEGER NOT NULL,"
        "  image_path TEXT,"
        "  ocr_done INTEGER NOT NULL DEFAULT 0,"
        "  UNIQUE(library_id, page_no)"
        ");"
        "CREATE TABLE IF NOT EXISTS text_blobs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  page_id INTEGER NOT NULL REFERENCES pages(id) ON DELETE CASCADE,"
        "  idx INTEGER NOT NULL,"
        "  bbox TEXT,"
        "  original_text TEXT NOT NULL,"
        "  text_hash TEXT NOT NULL,"
        "  UNIQUE(page_id, idx)"
        ");"
        "CREATE TABLE IF NOT EXISTS translations ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  text_hash TEXT NOT NULL,"
        "  target_lang TEXT NOT NULL,"
        "  engine TEXT NOT NULL,"
        "  translated_text TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
        "  UNIQUE(text_hash, target_lang, engine)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_blobs_hash    ON text_blobs(text_hash);"
        "CREATE INDEX IF NOT EXISTS idx_blobs_page    ON text_blobs(page_id);"
        "CREATE INDEX IF NOT EXISTS idx_pages_library ON pages(library_id);"
        "CREATE INDEX IF NOT EXISTS idx_trans_lookup  ON translations(text_hash, target_lang);";

    if (exec(db, schema) != SQLITE_OK) return -1;

    char buf[128];
    snprintf(buf, sizeof buf,
             "DELETE FROM schema_version; INSERT INTO schema_version VALUES (%d);",
             SCHEMA_VERSION);
    return exec(db, buf) == SQLITE_OK ? 0 : -1;
}

int db_open(Db *db, const char *path) {
    db->handle = NULL;
    int rc = sqlite3_open(path, &db->handle);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db_open: %s\n", sqlite3_errmsg(db->handle));
        return rc;
    }
    exec(db, "PRAGMA foreign_keys = ON;");
    exec(db, "PRAGMA journal_mode = WAL;");
    exec(db, "PRAGMA synchronous = NORMAL;");
    if (migrate(db) != 0) return SQLITE_ERROR;
    return SQLITE_OK;
}

void db_close(Db *db) {
    if (db->handle) sqlite3_close(db->handle);
    db->handle = NULL;
}

int db_library_upsert(Db *db, const char *path, const char *kind,
                      const char *title, int64_t *out_id) {
    const char *sql =
        "INSERT INTO library (path, kind, title, last_opened_at) "
        "VALUES (?, ?, ?, strftime('%s','now')) "
        "ON CONFLICT(path) DO UPDATE SET last_opened_at = strftime('%s','now');";
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &s, NULL) != SQLITE_OK) return SQLITE_ERROR;
    sqlite3_bind_text(s, 1, path, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, kind, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 3, title, -1, SQLITE_STATIC);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    if (rc != SQLITE_DONE) return rc;

    if (out_id) {
        sqlite3_stmt *q;
        if (sqlite3_prepare_v2(db->handle, "SELECT id FROM library WHERE path = ?;", -1, &q, NULL) != SQLITE_OK)
            return SQLITE_ERROR;
        sqlite3_bind_text(q, 1, path, -1, SQLITE_STATIC);
        if (sqlite3_step(q) == SQLITE_ROW) *out_id = sqlite3_column_int64(q, 0);
        sqlite3_finalize(q);
    }
    return SQLITE_OK;
}

int db_library_set_progress(Db *db, int64_t id, int page) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->handle,
            "UPDATE library SET last_page = ?, last_opened_at = strftime('%s','now') WHERE id = ?;",
            -1, &s, NULL) != SQLITE_OK) return SQLITE_ERROR;
    sqlite3_bind_int(s, 1, page);
    sqlite3_bind_int64(s, 2, id);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_library_get_progress(Db *db, const char *path) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->handle,
            "SELECT last_page FROM library WHERE path = ?;", -1, &s, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(s, 1, path, -1, SQLITE_STATIC);
    int page = -1;
    if (sqlite3_step(s) == SQLITE_ROW) page = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return page;
}

int db_library_delete(Db *db, int64_t id) {
    sqlite3_stmt *s;
    if (sqlite3_prepare_v2(db->handle,
            "DELETE FROM library WHERE id = ?;", -1, &s, NULL) != SQLITE_OK)
        return SQLITE_ERROR;
    sqlite3_bind_int64(s, 1, id);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_library_list(Db *db, sqlite3_stmt **stmt) {
    const char *sql =
        "SELECT id, kind, path, title, last_page, last_opened_at "
        "FROM library ORDER BY last_opened_at DESC NULLS LAST;";
    return sqlite3_prepare_v2(db->handle, sql, -1, stmt, NULL);
}

int db_page_upsert(Db *db, int64_t lib_id, int page_no,
                   const char *image_path, int64_t *out_id) {
    sqlite3_stmt *s;
    const char *sql =
        "INSERT INTO pages (library_id, page_no, image_path) VALUES (?, ?, ?) "
        "ON CONFLICT(library_id, page_no) DO UPDATE SET image_path = excluded.image_path;";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &s, NULL) != SQLITE_OK) return SQLITE_ERROR;
    sqlite3_bind_int64(s, 1, lib_id);
    sqlite3_bind_int(s, 2, page_no);
    if (image_path) sqlite3_bind_text(s, 3, image_path, -1, SQLITE_STATIC);
    else sqlite3_bind_null(s, 3);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    if (rc != SQLITE_DONE) return rc;

    if (out_id) {
        sqlite3_stmt *q;
        if (sqlite3_prepare_v2(db->handle,
                "SELECT id FROM pages WHERE library_id = ? AND page_no = ?;",
                -1, &q, NULL) != SQLITE_OK) return SQLITE_ERROR;
        sqlite3_bind_int64(q, 1, lib_id);
        sqlite3_bind_int(q, 2, page_no);
        if (sqlite3_step(q) == SQLITE_ROW) *out_id = sqlite3_column_int64(q, 0);
        sqlite3_finalize(q);
    }
    return SQLITE_OK;
}

int db_blob_add(Db *db, int64_t page_id, int idx,
                const char *bbox, const char *text, int64_t *out_id) {
    char hash[17];
    db_text_hash(text, hash);

    sqlite3_stmt *s;
    const char *sql =
        "INSERT INTO text_blobs (page_id, idx, bbox, original_text, text_hash) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(page_id, idx) DO UPDATE SET "
        "  bbox = excluded.bbox, original_text = excluded.original_text, text_hash = excluded.text_hash;";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &s, NULL) != SQLITE_OK) return SQLITE_ERROR;
    sqlite3_bind_int64(s, 1, page_id);
    sqlite3_bind_int(s, 2, idx);
    if (bbox) sqlite3_bind_text(s, 3, bbox, -1, SQLITE_STATIC);
    else sqlite3_bind_null(s, 3);
    sqlite3_bind_text(s, 4, text, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 5, hash, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    if (rc != SQLITE_DONE) return rc;

    if (out_id) {
        sqlite3_stmt *q;
        if (sqlite3_prepare_v2(db->handle,
                "SELECT id FROM text_blobs WHERE page_id = ? AND idx = ?;",
                -1, &q, NULL) != SQLITE_OK) return SQLITE_ERROR;
        sqlite3_bind_int64(q, 1, page_id);
        sqlite3_bind_int(q, 2, idx);
        if (sqlite3_step(q) == SQLITE_ROW) *out_id = sqlite3_column_int64(q, 0);
        sqlite3_finalize(q);
    }
    return SQLITE_OK;
}

int db_blobs_for_page(Db *db, int64_t page_id, sqlite3_stmt **stmt) {
    const char *sql =
        "SELECT id, idx, bbox, original_text, text_hash FROM text_blobs "
        "WHERE page_id = ? ORDER BY idx;";
    if (sqlite3_prepare_v2(db->handle, sql, -1, stmt, NULL) != SQLITE_OK) return SQLITE_ERROR;
    sqlite3_bind_int64(*stmt, 1, page_id);
    return SQLITE_OK;
}

int db_translation_get(Db *db, const char *text_hash, const char *lang,
                       char *out, size_t out_sz) {
    sqlite3_stmt *s;
    const char *sql =
        "SELECT translated_text FROM translations "
        "WHERE text_hash = ? AND target_lang = ? "
        "ORDER BY created_at DESC LIMIT 1;";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &s, NULL) != SQLITE_OK) return SQLITE_ERROR;
    sqlite3_bind_text(s, 1, text_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, lang, -1, SQLITE_STATIC);

    int rc = sqlite3_step(s);
    if (rc == SQLITE_ROW) {
        const unsigned char *t = sqlite3_column_text(s, 0);
        if (t && out && out_sz) {
            snprintf(out, out_sz, "%s", (const char *)t);
        }
        sqlite3_finalize(s);
        return SQLITE_OK;
    }
    sqlite3_finalize(s);
    return SQLITE_NOTFOUND;
}

int db_translation_put(Db *db, const char *text_hash, const char *lang,
                       const char *engine, const char *translated) {
    sqlite3_stmt *s;
    const char *sql =
        "INSERT INTO translations (text_hash, target_lang, engine, translated_text) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT(text_hash, target_lang, engine) DO UPDATE SET "
        "  translated_text = excluded.translated_text, created_at = strftime('%s','now');";
    if (sqlite3_prepare_v2(db->handle, sql, -1, &s, NULL) != SQLITE_OK) return SQLITE_ERROR;
    sqlite3_bind_text(s, 1, text_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, lang, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 3, engine, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 4, translated, -1, SQLITE_STATIC);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

void db_text_hash(const char *text, char out[17]) {
    /* FNV-1a 64-bit */
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        h ^= *p;
        h *= 0x100000001b3ULL;
    }
    snprintf(out, 17, "%016llx", (unsigned long long)h);
}
