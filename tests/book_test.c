#include "book_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define RES "tests/res"

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  PASS %s\n", msg); \
    else { printf("  FAIL %s\n", msg); fails++; } \
} while (0)

static const char *fixture(const char *name, const char *content) {
    static char path[256];
    mkdir(RES, 0755);
    snprintf(path, sizeof path, RES "/%s", name);
    FILE *f = fopen(path, "w");
    fputs(content, f);
    fclose(f);
    return path;
}

static void test_load_errors(void) {
    printf("test_load_errors\n");
    Book b;
    CHECK(book_load(&b, RES "/no_such_file.txt") == 0, "missing file fails");
    CHECK(book_load(&b, fixture("empty.txt", "")) == 0, "empty file fails");
    CHECK(book_page(&b)[0] == '\0', "empty book page is \"\"");
    CHECK(book_index(&b) == 0, "empty book index 0");
}

static void test_single_page(void) {
    printf("test_single_page\n");
    Book b;
    CHECK(book_load(&b, fixture("plain.txt", "one\ntwo\n")) == 1, "load ok");
    CHECK(book_count(&b) == 1, "no markers -> one page");
    CHECK(strcmp(book_page(&b), "one\ntwo\n") == 0, "content preserved");
    CHECK(book_index(&b) == 1, "index is 1-based");
    book_free(&b);
    CHECK(b.pages == NULL && b.count == 0, "free resets");
}

static void test_page_breaks(void) {
    printf("test_page_breaks\n");
    Book b;
    book_load(&b, fixture("paged.txt", "one\n<PAGE_BREAK>\ntwo\n<PAGE_BREAK>\nthree\n"));
    CHECK(book_count(&b) == 3, "three pages");
    CHECK(strcmp(book_page(&b), "one\n") == 0, "page 1");
    book_next(&b);
    CHECK(strcmp(book_page(&b), "two\n") == 0, "page 2");
    book_next(&b);
    CHECK(strcmp(book_page(&b), "three\n") == 0, "page 3");
    book_next(&b);
    CHECK(book_index(&b) == 3, "next clamps at last");
    book_prev(&b);
    book_prev(&b);
    book_prev(&b);
    CHECK(book_index(&b) == 1, "prev clamps at first");
    book_free(&b);
}

static void test_break_edges(void) {
    printf("test_break_edges\n");
    Book b;
    book_load(&b, fixture("edges.txt",
                          "<PAGE_BREAK>\na\n<PAGE_BREAK>\n<PAGE_BREAK>\nb\n<PAGE_BREAK>\n"));
    /* leading and doubled markers make empty pages, trailing one doesn't */
    CHECK(book_count(&b) == 4, "empty pages kept, no trailing page");
    CHECK(strcmp(book_page(&b), "") == 0, "leading marker -> empty first page");
    book_next(&b); book_next(&b);
    CHECK(strcmp(book_page(&b), "") == 0, "doubled marker -> empty page");
    book_next(&b);
    CHECK(strcmp(book_page(&b), "b\n") == 0, "last page content");
    book_free(&b);

    book_load(&b, fixture("inline.txt", "x <PAGE_BREAK> y\n"));
    CHECK(book_count(&b) == 1, "marker inside a line is not a break");
    book_free(&b);
}

static void test_long_line(void) {
    printf("test_long_line\n");
    char content[5002];
    memset(content, 'a', 5000);
    content[5000] = '\n';
    content[5001] = '\0';

    Book b;
    book_load(&b, fixture("long.txt", content));
    CHECK(book_count(&b) == 1, "one page");
    const char *p = book_page(&b);
    CHECK(strlen(p) == 5001, "length intact across fgets chunks");
    CHECK(strchr(p, '\n') == p + 5000, "no newline injected mid-line");
    book_free(&b);
}

static void test_no_trailing_newline(void) {
    printf("test_no_trailing_newline\n");
    Book b;
    book_load(&b, fixture("notrail.txt", "abc"));
    CHECK(book_count(&b) == 1, "one page");
    CHECK(strcmp(book_page(&b), "abc") == 0, "content without trailing newline");
    book_free(&b);
}

int main(void) {
    test_load_errors();
    test_single_page();
    test_page_breaks();
    test_break_edges();
    test_long_line();
    test_no_trailing_newline();
    printf("\n%s (%d failures)\n", fails ? "FAILED" : "OK", fails);
    return fails ? 1 : 0;
}
