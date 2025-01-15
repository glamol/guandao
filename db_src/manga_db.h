#ifndef MANGA_DB_H
#define MANGA_DB_H

#include <sqlite3.h>

// Initialize the database and create necessary tables
// Parameters:
//   db - Pointer to the SQLite database connection
//   db_path - Path to the SQLite database file
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int db_init(sqlite3 **db, const char *db_path);

// Add a new manga to the database
// Parameters:
//   db - SQLite database connection
//   title - Title of the manga
//   author - Author of the manga
//   description - Description of the manga
//   cover_image_path - Path to the cover image of the manga
//   manga_id - Pointer to store the new manga ID
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int add_manga(sqlite3 *db, const char *title, const char *author, const char *description, const char *cover_image_path, int *manga_id);

// Add a new page to a manga
// Parameters:
//   db - SQLite database connection
//   manga_id - ID of the manga to which the page belongs
//   page_number - Page number in the manga
//   image_path - Path to the image file for the page
//   page_id - Pointer to store the new page ID
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int add_page(sqlite3 *db, int manga_id, int page_number, const char *image_path, int *page_id);

// Add a text blob to a page
// Parameters:
//   db - SQLite database connection
//   page_id - ID of the page to which the text blob belongs
//   position - Position of the blob in JSON or string format
//   original_text - Original text in the blob
//   blob_id - Pointer to store the new blob ID
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int add_text_blob(sqlite3 *db, int page_id, const char *position, const char *original_text, int *blob_id);

// Add a translation for a text blob
// Parameters:
//   db - SQLite database connection
//   blob_id - ID of the text blob to translate
//   language - Language code of the translation (e.g., "en", "jp")
//   translated_text - Translated text
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int add_translation(sqlite3 *db, int blob_id, const char *language, const char *translated_text);

// Retrieve all manga from the database
// Parameters:
//   db - SQLite database connection
//   stmt - Pointer to an SQLite statement to iterate over results
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int get_manga(sqlite3 *db, sqlite3_stmt **stmt);

// Retrieve all pages for a specific manga
// Parameters:
//   db - SQLite database connection
//   manga_id - ID of the manga
//   stmt - Pointer to an SQLite statement to iterate over results
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int get_pages(sqlite3 *db, int manga_id, sqlite3_stmt **stmt);

// Retrieve all text blobs for a specific page
// Parameters:
//   db - SQLite database connection
//   page_id - ID of the page
//   stmt - Pointer to an SQLite statement to iterate over results
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int get_text_blobs(sqlite3 *db, int page_id, sqlite3_stmt **stmt);

// Retrieve translations for a specific text blob
// Parameters:
//   db - SQLite database connection
//   blob_id - ID of the text blob
//   stmt - Pointer to an SQLite statement to iterate over results
// Returns:
//   SQLITE_OK on success, or an SQLite error code on failure
int get_translations(sqlite3 *db, int blob_id, sqlite3_stmt **stmt);

// Close the database connection
// Parameters:
//   db - SQLite database connection
void db_close(sqlite3 *db);

#endif
