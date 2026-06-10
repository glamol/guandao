#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_DB "tests/res/test.db"

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  PASS %s\n", msg); \
    else { printf("  FAIL %s\n", msg); fails++; } \
} while (0)

static void reset(void) { unlink(TEST_DB); unlink(TEST_DB "-wal"); unlink(TEST_DB "-shm"); }

static void test_open(void) {
    printf("test_open\n");
    reset();
    Db db;
    CHECK(db_open(&db, TEST_DB) == SQLITE_OK, "open");
    db_close(&db);
}

static void test_library(void) {
    printf("test_library\n");
    reset();
    Db db; db_open(&db, TEST_DB);

    int64_t id = 0;
    CHECK(db_library_upsert(&db, "/tmp/a.txt", "book", "A", &id) == SQLITE_OK, "upsert");
    CHECK(id > 0, "id assigned");

    CHECK(db_library_get_progress(&db, "/tmp/a.txt") == 0, "default progress 0");
    CHECK(db_library_get_progress(&db, "/tmp/missing") == -1, "missing -> -1");

    CHECK(db_library_set_progress(&db, id, 42) == SQLITE_OK, "set progress");
    CHECK(db_library_get_progress(&db, "/tmp/a.txt") == 42, "read progress");

    int64_t id2 = 0;
    db_library_upsert(&db, "/tmp/a.txt", "book", "A", &id2);
    CHECK(id2 == id, "upsert is idempotent");
    CHECK(db_library_get_progress(&db, "/tmp/a.txt") == 42, "upsert preserves progress");

    db_close(&db);
}

static void test_pages_blobs(void) {
    printf("test_pages_blobs\n");
    reset();
    Db db; db_open(&db, TEST_DB);

    int64_t lib_id; db_library_upsert(&db, "/tmp/m", "manga", "M", &lib_id);

    int64_t p0 = 0, p1 = 0;
    db_page_upsert(&db, lib_id, 0, "/tmp/m/00.png", &p0);
    db_page_upsert(&db, lib_id, 1, "/tmp/m/01.png", &p1);
    CHECK(p0 > 0 && p1 > 0 && p0 != p1, "two distinct pages");

    int64_t b0 = 0;
    CHECK(db_blob_add(&db, p0, 0, "{\"x\":1,\"y\":2}", "hello", &b0) == SQLITE_OK, "blob add");
    CHECK(b0 > 0, "blob id");

    sqlite3_stmt *s;
    db_blobs_for_page(&db, p0, &s);
    int rows = 0;
    while (sqlite3_step(s) == SQLITE_ROW) rows++;
    sqlite3_finalize(s);
    CHECK(rows == 1, "blob enumeration");

    db_close(&db);
}

static void test_translations(void) {
    printf("test_translations\n");
    reset();
    Db db; db_open(&db, TEST_DB);

    char h1[17], h2[17];
    db_text_hash("こんにちは", h1);
    db_text_hash("こんにちは", h2);
    CHECK(strcmp(h1, h2) == 0, "hash deterministic");

    char h3[17];
    db_text_hash("different", h3);
    CHECK(strcmp(h1, h3) != 0, "hash differs");

    char buf[256] = "";
    CHECK(db_translation_get(&db, h1, "ru", buf, sizeof buf) == SQLITE_NOTFOUND, "miss");

    db_translation_put(&db, h1, "ru", "manual", "привет");
    CHECK(db_translation_get(&db, h1, "ru", buf, sizeof buf) == SQLITE_OK, "hit");
    CHECK(strcmp(buf, "привет") == 0, "value correct");

    db_translation_put(&db, h1, "en", "manual", "hello");
    CHECK(db_translation_get(&db, h1, "en", buf, sizeof buf) == SQLITE_OK, "second lang");
    CHECK(strcmp(buf, "hello") == 0, "en value");

    db_close(&db);
}

static void test_delete_cascade(void) {
    printf("test_delete_cascade\n");
    reset();
    Db db; db_open(&db, TEST_DB);

    int64_t lib_id, page_id, blob_id;
    db_library_upsert(&db, "/tmp/d.cbz", "manga", "D", &lib_id);
    db_page_upsert(&db, lib_id, 0, "/tmp/d/00.png", &page_id);
    db_blob_add(&db, page_id, 0, NULL, "text", &blob_id);

    CHECK(db_library_delete(&db, lib_id) == SQLITE_OK, "delete");
    CHECK(db_library_get_progress(&db, "/tmp/d.cbz") == -1, "entry gone");

    sqlite3_stmt *s;
    int pages = 0;
    sqlite3_prepare_v2(db.handle, "SELECT COUNT(*) FROM pages;", -1, &s, NULL);
    if (sqlite3_step(s) == SQLITE_ROW) pages = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    CHECK(pages == 0, "pages cascade");

    int blobs = 0;
    sqlite3_prepare_v2(db.handle, "SELECT COUNT(*) FROM text_blobs;", -1, &s, NULL);
    if (sqlite3_step(s) == SQLITE_ROW) blobs = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    CHECK(blobs == 0, "blobs cascade");

    db_close(&db);
}

static void test_list_order(void) {
    printf("test_list_order\n");
    reset();
    Db db; db_open(&db, TEST_DB);

    int64_t a, b;
    db_library_upsert(&db, "/tmp/old.txt", "book", "Old", &a);
    db_library_upsert(&db, "/tmp/new.txt", "book", "New", &b);
    /* last_opened_at has second resolution, force distinct values */
    char sql[128];
    snprintf(sql, sizeof sql, "UPDATE library SET last_opened_at = 100 WHERE id = %lld;", (long long)a);
    sqlite3_exec(db.handle, sql, NULL, NULL, NULL);
    snprintf(sql, sizeof sql, "UPDATE library SET last_opened_at = 200 WHERE id = %lld;", (long long)b);
    sqlite3_exec(db.handle, sql, NULL, NULL, NULL);

    sqlite3_stmt *s;
    CHECK(db_library_list(&db, &s) == SQLITE_OK, "list");
    CHECK(sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int64(s, 0) == b, "most recent first");
    CHECK(sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int64(s, 0) == a, "older second");
    sqlite3_finalize(s);

    db_close(&db);
}

int main(void) {
    test_open();
    test_library();
    test_pages_blobs();
    test_translations();
    test_delete_cascade();
    test_list_order();
    printf("\n%s (%d failures)\n", fails ? "FAILED" : "OK", fails);
    return fails ? 1 : 0;
}
