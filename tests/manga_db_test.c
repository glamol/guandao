#include "manga_db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB_FILE "tests/res/test_manga.db"

void test_db_init() {
    sqlite3 *db = NULL;
    printf("Testing db_init...\n");
    if (db_init(&db, TEST_DB_FILE) == SQLITE_OK) {
        printf("db_init: PASS\n");
    } else {
        printf("db_init: FAIL\n");
    }
    db_close(db);
}

void test_add_manga() {
    sqlite3 *db = NULL;
    db_init(&db, TEST_DB_FILE);

    printf("Testing add_manga...\n");
    int manga_id;
    if (add_manga(db, "Test Manga", "Test Author", "A test manga description", "test_cover.jpg", &manga_id) == SQLITE_OK) {
        printf("add_manga: PASS (Manga ID: %d)\n", manga_id);
    } else {
        printf("add_manga: FAIL\n");
    }

    db_close(db);
}

void test_add_page() {
    sqlite3 *db = NULL;
    db_init(&db, TEST_DB_FILE);

    int manga_id;
    add_manga(db, "Test Manga", "Test Author", "A test manga description", "test_cover.jpg", &manga_id);

    printf("Testing add_page...\n");
    int page_id;
    if (add_page(db, manga_id, 1, "test_page1.jpg", &page_id) == SQLITE_OK) {
        printf("add_page: PASS (Page ID: %d)\n", page_id);
    } else {
        printf("add_page: FAIL\n");
    }

    db_close(db);
}

void test_add_text_blob() {
    sqlite3 *db = NULL;
    db_init(&db, TEST_DB_FILE);

    int manga_id, page_id;
    add_manga(db, "Test Manga", "Test Author", "A test manga description", "test_cover.jpg", &manga_id);
    add_page(db, manga_id, 1, "test_page1.jpg", &page_id);

    printf("Testing add_text_blob...\n");
    int blob_id;
    if (add_text_blob(db, page_id, "{x:10,y:20,width:100,height:50}", "Hello World", &blob_id) == SQLITE_OK) {
        printf("add_text_blob: PASS (Blob ID: %d)\n", blob_id);
    } else {
        printf("add_text_blob: FAIL\n");
    }

    db_close(db);
}

void test_add_translation() {
    sqlite3 *db = NULL;
    db_init(&db, TEST_DB_FILE);

    int manga_id, page_id, blob_id;
    add_manga(db, "Test Manga", "Test Author", "A test manga description", "test_cover.jpg", &manga_id);
    add_page(db, manga_id, 1, "test_page1.jpg", &page_id);
    add_text_blob(db, page_id, "{x:10,y:20,width:100,height:50}", "Hello World", &blob_id);

    printf("Testing add_translation...\n");
    if (add_translation(db, blob_id, "en", "Hello World Translated") == SQLITE_OK) {
        printf("add_translation: PASS\n");
    } else {
        printf("add_translation: FAIL\n");
    }

    db_close(db);
}

void test_get_manga() {
    sqlite3 *db = NULL;
    db_init(&db, TEST_DB_FILE);

    int manga_id;
    add_manga(db, "Test Manga", "Test Author", "A test manga description", "test_cover.jpg", &manga_id);

    printf("Testing get_manga...\n");
    sqlite3_stmt *stmt;
    if (get_manga(db, &stmt) == SQLITE_OK) {
        printf("get_manga: PASS\n");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  Manga: %s (Author: %s)\n",
                   sqlite3_column_text(stmt, 1),
                   sqlite3_column_text(stmt, 2));
        }
        sqlite3_finalize(stmt);
    } else {
        printf("get_manga: FAIL\n");
    }

    db_close(db);
}

int main() {
    test_db_init();
    test_add_manga();
    test_add_page();
    test_add_text_blob();
    test_add_translation();
    test_get_manga();

    printf("Tests complete.\n");
    return 0;
}
