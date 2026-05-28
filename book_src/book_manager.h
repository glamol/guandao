#ifndef BOOK_MANAGER_H
#define BOOK_MANAGER_H

#include <stddef.h>

typedef struct {
    char **pages;
    size_t count;
    size_t cur;
} Book;

int  book_load(Book *b, const char *path);
void book_free(Book *b);

const char *book_page(const Book *b);
size_t      book_count(const Book *b);
size_t      book_index(const Book *b);

void book_next(Book *b);
void book_prev(Book *b);

#endif
