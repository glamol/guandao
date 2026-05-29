#include "cbz.h"
#include "miniz.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define FIXTURE  "tests/res/sample.cbz"
#define EVIL     "tests/res/evil.cbz"
#define CACHE    "tests/res/cache"

static int fails = 0;
#define CHECK(cond, msg) do { \
    if (cond) printf("  PASS %s\n", msg); \
    else { printf("  FAIL %s\n", msg); fails++; } \
} while (0)

/* tiny valid PNG (1x1 red) */
static const unsigned char PNG1[] = {
    0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A, 0x00,0x00,0x00,0x0D,
    0x49,0x48,0x44,0x52, 0x00,0x00,0x00,0x01, 0x00,0x00,0x00,0x01,
    0x08,0x02,0x00,0x00,0x00, 0x90,0x77,0x53,0xDE, 0x00,0x00,0x00,0x0C,
    0x49,0x44,0x41,0x54, 0x08,0x99,0x63,0xF8,0xCF,0xC0,0x00,0x00,0x00,
    0x03,0x00,0x01, 0x5B,0xB6,0xEE,0x56, 0x00,0x00,0x00,0x00,
    0x49,0x45,0x4E,0x44, 0xAE,0x42,0x60,0x82,
};

static void rm_rf(const char *path) {
    char cmd[512];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", path);
    (void)system(cmd);
}

static int count_files(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        n++;
    }
    closedir(d);
    return n;
}

static int file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static void build_fixture(const char *path, int evil) {
    remove(path);
    mz_zip_archive z = {0};
    mz_zip_writer_init_file(&z, path, 0);
    mz_zip_writer_add_mem(&z, "page_01.png", PNG1, sizeof PNG1, MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_add_mem(&z, "page_02.png", PNG1, sizeof PNG1, MZ_DEFAULT_COMPRESSION);
    mz_zip_writer_add_mem(&z, "readme.txt", "hi", 2, MZ_DEFAULT_COMPRESSION);
    if (evil) {
        mz_zip_writer_add_mem(&z, "../escape.png", PNG1, sizeof PNG1, MZ_DEFAULT_COMPRESSION);
    }
    mz_zip_writer_finalize_archive(&z);
    mz_zip_writer_end(&z);
}

static void test_extract(void) {
    printf("test_extract\n");
    rm_rf(CACHE);
    mkdir("tests/res", 0755);
    build_fixture(FIXTURE, 0);

    char out[512];
    int rc = cbz_extract(FIXTURE, CACHE, out, sizeof out);
    CHECK(rc == 0, "extract ok");
    CHECK(file_exists(out), "out dir exists");

    int n = count_files(out);
    CHECK(n == 2, "only image files extracted (no readme.txt)");

    char p[600];
    snprintf(p, sizeof p, "%s/page_01.png", out);
    CHECK(file_exists(p), "page_01.png present");
}

static void test_idempotent(void) {
    printf("test_idempotent\n");
    char out1[512], out2[512];
    cbz_extract(FIXTURE, CACHE, out1, sizeof out1);
    cbz_extract(FIXTURE, CACHE, out2, sizeof out2);
    CHECK(strcmp(out1, out2) == 0, "same cache dir");
}

static void test_safety(void) {
    printf("test_safety\n");
    rm_rf(CACHE);
    build_fixture(EVIL, 1);

    char out[512];
    cbz_extract(EVIL, CACHE, out, sizeof out);

    CHECK(!file_exists("tests/res/escape.png"), "no traversal write");
    CHECK(!file_exists("tests/escape.png"), "no parent traversal");
}

int main(void) {
    test_extract();
    test_idempotent();
    test_safety();
    printf("\n%s (%d failures)\n", fails ? "FAILED" : "OK", fails);
    return fails ? 1 : 0;
}
