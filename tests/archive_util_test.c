#include "archive_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define RES "tests/res/au"

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  PASS %s\n", msg); \
    else { printf("  FAIL %s\n", msg); fails++; } \
} while (0)

static int dir_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static void test_hash(void) {
    printf("test_hash\n");
    char a[17], b[17], c[17];
    au_hash_hex("/some/path.cbz", a);
    au_hash_hex("/some/path.cbz", b);
    au_hash_hex("/other/path.cbz", c);
    CHECK(strcmp(a, b) == 0, "deterministic");
    CHECK(strcmp(a, c) != 0, "differs by input");
    CHECK(strlen(a) == 16, "16 hex chars");
    CHECK(strspn(a, "0123456789abcdef") == 16, "lowercase hex only");
}

static void test_img_ext(void) {
    printf("test_img_ext\n");
    CHECK(au_has_img_ext("a.png"), "png");
    CHECK(au_has_img_ext("a.JPG"), "JPG case-insensitive");
    CHECK(au_has_img_ext("a.JpEg"), "jpeg mixed case");
    CHECK(au_has_img_ext("a.webp"), "webp");
    CHECK(!au_has_img_ext("a.txt"), "txt rejected");
    CHECK(!au_has_img_ext("noext"), "no extension rejected");
    CHECK(!au_has_img_ext("a.png.txt"), "last extension wins");
}

static void test_safe_name(void) {
    printf("test_safe_name\n");
    CHECK(au_safe_name("page.png"), "plain name");
    CHECK(au_safe_name("dir/page.png"), "nested name");
    CHECK(!au_safe_name("../escape.png"), "parent traversal rejected");
    CHECK(!au_safe_name("a/../b.png"), "inner traversal rejected");
    CHECK(!au_safe_name("/abs.png"), "absolute rejected");
    CHECK(!au_safe_name(""), "empty rejected");
    CHECK(!au_safe_name(NULL), "NULL rejected");
}

static void test_mkpath_done(void) {
    printf("test_mkpath_done\n");
    char cmd[256];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", RES);
    (void)system(cmd);

    CHECK(au_mkpath(RES "/x/y/z") == 0, "mkpath nested");
    CHECK(dir_exists(RES "/x/y/z"), "dirs created");
    CHECK(au_mkpath(RES "/x/y/z") == 0, "mkpath idempotent");
    CHECK(au_mkpath(RES "/x/y/z/") == 0, "trailing slash ok");

    CHECK(!au_dir_has_done(RES "/x/y/z"), "fresh dir not done");
    au_mark_done(RES "/x/y/z");
    CHECK(au_dir_has_done(RES "/x/y/z"), "marked done");
}

static void test_basename(void) {
    printf("test_basename\n");
    CHECK(strcmp(au_basename("a/b/c.png"), "c.png") == 0, "nested");
    CHECK(strcmp(au_basename("c.png"), "c.png") == 0, "no slash");
    CHECK(strcmp(au_basename("a/"), "") == 0, "trailing slash");
}

int main(void) {
    test_hash();
    test_img_ext();
    test_safe_name();
    test_mkpath_done();
    test_basename();
    printf("\n%s (%d failures)\n", fails ? "FAILED" : "OK", fails);
    return fails ? 1 : 0;
}
