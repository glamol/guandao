#include "book_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_BREAK "<PAGE_BREAK>"

static int push_page(Book *b, char *page) {
    char **grown = realloc(b->pages, (b->count + 1) * sizeof(*grown));
    if (!grown) { free(page); return 0; }
    b->pages = grown;
    b->pages[b->count++] = page;
    return 1;
}

static char *buf_append(char *buf, size_t *len, size_t *cap, const char *s, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 256;
        while (nc < *len + n + 1) nc *= 2;
        char *g = realloc(buf, nc);
        if (!g) { free(buf); return NULL; }
        buf = g;
        *cap = nc;
    }
    memcpy(buf + *len, s, n);
    *len += n;
    buf[*len] = '\0';
    return buf;
}

int book_load(Book *b, const char *path) {
    b->pages = NULL;
    b->count = 0;
    b->cur = 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    char *page = NULL;
    size_t len = 0, cap = 0;

    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        int complete = (n > 0 && line[n - 1] == '\n');
        if (complete) line[--n] = '\0';

        if (complete && strcmp(line, PAGE_BREAK) == 0) {
            if (!page) { page = calloc(1, 1); if (!page) { fclose(f); book_free(b); return 0; } }
            if (!push_page(b, page)) { fclose(f); book_free(b); return 0; }
            page = NULL; len = 0; cap = 0;
            continue;
        }

        page = buf_append(page, &len, &cap, line, n);
        if (!page) { fclose(f); book_free(b); return 0; }
        if (complete) {
            page = buf_append(page, &len, &cap, "\n", 1);
            if (!page) { fclose(f); book_free(b); return 0; }
        }
    }
    fclose(f);

    if (page && len > 0) {
        if (!push_page(b, page)) { book_free(b); return 0; }
    } else {
        free(page);
    }

    return b->count > 0;
}

void book_free(Book *b) {
    for (size_t i = 0; i < b->count; i++) free(b->pages[i]);
    free(b->pages);
    b->pages = NULL;
    b->count = 0;
    b->cur = 0;
}

const char *book_page(const Book *b) {
    return b->count ? b->pages[b->cur] : "";
}

size_t book_count(const Book *b) { return b->count; }
size_t book_index(const Book *b) { return b->count ? b->cur + 1 : 0; }

void book_next(Book *b) {
    if (b->count && b->cur + 1 < b->count) b->cur++;
}

void book_prev(Book *b) {
    if (b->cur > 0) b->cur--;
}
