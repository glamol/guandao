#include "manga_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_sql_error(int rc, sqlite3 *db, const char *msg) {
    if (rc != SQLITE_OK) {
        fprintf(stderr, "%s: %s\n", msg, sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
}

int db_init(sqlite3 **db, const char *db_path) {
    int rc = sqlite3_open(db_path, db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
        return rc;
    }

    const char *create_tables =
        "CREATE TABLE IF NOT EXISTS manga ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  author TEXT,"
        "  description TEXT,"
        "  cover_image_path TEXT,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS pages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  manga_id INTEGER NOT NULL,"
        "  page_number INTEGER NOT NULL,"
        "  image_path TEXT NOT NULL,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (manga_id) REFERENCES manga (id)"
        ");"
        "CREATE TABLE IF NOT EXISTS text_blobs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  page_id INTEGER NOT NULL,"
        "  position TEXT NOT NULL,"
        "  original_text TEXT NOT NULL,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (page_id) REFERENCES pages (id)"
        ");"
        "CREATE TABLE IF NOT EXISTS translations ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  text_blob_id INTEGER NOT NULL,"
        "  language TEXT NOT NULL,"
        "  translated_text TEXT NOT NULL,"
        "  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY (text_blob_id) REFERENCES text_blobs (id)"
        ");";

    check_sql_error(sqlite3_exec(*db, create_tables, NULL, NULL, NULL), *db, "Failed to create tables");
    return SQLITE_OK;
}

int add_manga(sqlite3 *db, const char *title, const char *author, const char *description, const char *cover_image_path, int *manga_id) {
    const char *sql = "INSERT INTO manga (title, author, description, cover_image_path) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL), db, "Failed to prepare add_manga");

    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, author, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, cover_image_path, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        *manga_id = sqlite3_last_insert_rowid(db);
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int add_page(sqlite3 *db, int manga_id, int page_number, const char *image_path, int *page_id) {
    const char *sql = "INSERT INTO pages (manga_id, page_number, image_path) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL), db, "Failed to prepare add_page");

    sqlite3_bind_int(stmt, 1, manga_id);
    sqlite3_bind_int(stmt, 2, page_number);
    sqlite3_bind_text(stmt, 3, image_path, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        *page_id = sqlite3_last_insert_rowid(db);
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int add_text_blob(sqlite3 *db, int page_id, const char *position, const char *original_text, int *blob_id) {
    const char *sql = "INSERT INTO text_blobs (page_id, position, original_text) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL), db, "Failed to prepare add_text_blob");

    sqlite3_bind_int(stmt, 1, page_id);
    sqlite3_bind_text(stmt, 2, position, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, original_text, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        *blob_id = sqlite3_last_insert_rowid(db);
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int add_translation(sqlite3 *db, int blob_id, const char *language, const char *translated_text) {
    const char *sql = "INSERT INTO translations (text_blob_id, language, translated_text) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL), db, "Failed to prepare add_translation");

    sqlite3_bind_int(stmt, 1, blob_id);
    sqlite3_bind_text(stmt, 2, language, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, translated_text, -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int get_translations(sqlite3 *db, int blob_id, sqlite3_stmt **stmt) {
    const char *sql = "SELECT language, translated_text FROM translations WHERE text_blob_id = ?;";
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, stmt, NULL), db, "Failed to prepare get_translations");

    sqlite3_bind_int(*stmt, 1, blob_id);
    return SQLITE_OK;
}

int get_manga(sqlite3 *db, sqlite3_stmt **stmt) {
    const char *sql = "SELECT id, title, author, description, cover_image_path FROM manga;";
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, stmt, NULL), db, "Failed to prepare get_manga");
    return SQLITE_OK;
}

int get_pages(sqlite3 *db, int manga_id, sqlite3_stmt **stmt) {
    const char *sql = "SELECT id, page_number, image_path FROM pages WHERE manga_id = ?;";
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, stmt, NULL), db, "Failed to prepare get_pages");

    sqlite3_bind_int(*stmt, 1, manga_id);
    return SQLITE_OK;
}

int get_text_blobs(sqlite3 *db, int page_id, sqlite3_stmt **stmt) {
    const char *sql = "SELECT id, position, original_text FROM text_blobs WHERE page_id = ?;";
    check_sql_error(sqlite3_prepare_v2(db, sql, -1, stmt, NULL), db, "Failed to prepare get_text_blobs");

    sqlite3_bind_int(*stmt, 1, page_id);
    return SQLITE_OK;
}

void db_close(sqlite3 *db) {
    sqlite3_close(db);
}
