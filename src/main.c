#include "raylib.h"
#include "tinyfiledialogs.h"
#include "book_manager.h"
#include "db.h"
#include "cbz.h"
#include "cbt.h"
#include "archive_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>

typedef struct {
    int screen_width;
    int screen_height;

    int panel_width;
    int panel_animation_speed;
    int panel_x;
    int panel_visible;
    int is_animating;

    int panel_toggle_button_width;
    int panel_toggle_button_height;
    int panel_toggle_button_offset;
    char panel_toggle_icon[2];
    Color panel_toggle_color;
    Color panel_toggle_text_color;

    int panel_content_button_offset;
    int panel_button_width;
    int panel_button_height;
    int open_button_y_offset;

    Book book;
    int book_loaded;
    int book_load_attempted;
    char book_path[1024];
    size_t saved_index;
    int64_t lib_id;

    Db db;

    /* library list */
    struct LibEntry {
        int64_t id;
        char kind[8];
        char path[1024];
        char title[256];
    } *lib_entries;
    size_t lib_count;
    size_t lib_cap;
    float lib_scroll;
    char page_info[64];

    char **image_paths;
    size_t image_count;
    size_t image_cur;
    int has_manga;
    struct MangaSlot {
        Texture2D tex;
        size_t idx;
        int valid;
    } manga_cache[3];

    size_t sub_page;
    size_t sub_count;
    size_t lines_per_page;
    int goto_last_sub;

    size_t cached_index;
    int cache_valid;
    float cached_width;
    char **wrapped;
    size_t wrapped_count;

    Font font;

    int exit_request;

    float text_margin_horizontal;
    float text_margin_top;
    float text_margin_bottom;
    int font_size;
    float text_line_spacing_factor;
    Rectangle text_display_area;

    float nav_button_width;
    float nav_button_height;
    float nav_button_margin;
    Rectangle prev_page_button_rect;
    Rectangle next_page_button_rect;
} AppContext;

static void free_lines(char **lines, size_t n) {
    for (size_t i = 0; i < n; i++) free(lines[i]);
    free(lines);
}

static int build_codepoints(int **out) {
    static const struct { int lo, hi; } ranges[] = {
        {0x0020, 0x007E},  // ASCII
        {0x00A0, 0x00FF},  // Latin-1 supplement
        {0x0400, 0x04FF},  // Cyrillic
        {0x3040, 0x309F},  // Hiragana
        {0x30A0, 0x30FF},  // Katakana
        {0x4E00, 0x9FAF},  // CJK Unified (common kanji)
        {0x3000, 0x303F},  // CJK symbols and punctuation
        {0xFF00, 0xFFEF},  // Halfwidth/fullwidth forms
    };
    size_t nr = sizeof ranges / sizeof ranges[0];
    size_t total = 0;
    for (size_t i = 0; i < nr; i++) total += ranges[i].hi - ranges[i].lo + 1;
    int *cps = malloc(total * sizeof *cps);
    size_t k = 0;
    for (size_t i = 0; i < nr; i++)
        for (int c = ranges[i].lo; c <= ranges[i].hi; c++) cps[k++] = c;
    *out = cps;
    return (int)total;
}

static Font load_app_font(int size) {
    const char *rel = "assets/fonts/GoNotoKurrent-Regular.ttf";
    const char *app_dir = GetApplicationDirectory();
    char candidates[3][1024];
    snprintf(candidates[0], sizeof candidates[0], "%s%s", app_dir, rel);
    snprintf(candidates[1], sizeof candidates[1], "%s../%s", app_dir, rel);
    snprintf(candidates[2], sizeof candidates[2], "%s", rel);

    int *cps; int n = build_codepoints(&cps);
    for (size_t i = 0; i < sizeof candidates / sizeof candidates[0]; i++) {
        if (FileExists(candidates[i])) {
            Font f = LoadFontEx(candidates[i], size, cps, n);
            free(cps);
            if (f.texture.id) {
                SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
                return f;
            }
        }
    }
    free(cps);
    TraceLog(LOG_WARNING, "font asset missing: %s", rel);
    return GetFontDefault();
}

static char **wrap_text(Font font, const char *text, float fs, float maxw, size_t *out_n) {
    char **lines = NULL;
    size_t n = 0;
    char current[4096] = "";
    size_t cl = 0;

    const char *p = text;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '\n') {
            char *line = malloc(cl + 1);
            memcpy(line, current, cl + 1);
            char **g = realloc(lines, (n + 1) * sizeof *g);
            lines = g; lines[n++] = line;
            current[0] = '\0'; cl = 0;
            p++;
            continue;
        }
        if (!*p) break;

        char word[1024];
        size_t wl = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && wl < sizeof word - 1) {
            word[wl++] = *p++;
        }
        word[wl] = '\0';

        char candidate[4096];
        if (cl == 0) snprintf(candidate, sizeof candidate, "%s", word);
        else snprintf(candidate, sizeof candidate, "%s %s", current, word);

        if (MeasureTextEx(font, candidate, fs, 1).x <= maxw) {
            size_t cn = strlen(candidate);
            memcpy(current, candidate, cn + 1);
            cl = cn;
        } else {
            if (cl > 0) {
                char *line = malloc(cl + 1);
                memcpy(line, current, cl + 1);
                char **g = realloc(lines, (n + 1) * sizeof *g);
                lines = g; lines[n++] = line;
            }
            memcpy(current, word, wl + 1);
            cl = wl;
        }
    }
    if (cl > 0) {
        char *line = malloc(cl + 1);
        memcpy(line, current, cl + 1);
        char **g = realloc(lines, (n + 1) * sizeof *g);
        lines = g; lines[n++] = line;
    }
    *out_n = n;
    return lines;
}

static void HandleInput(AppContext *ctx);
static void UpdateState(AppContext *ctx);
static void DrawUI(const AppContext *ctx);
static void refresh_library(AppContext *ctx);
static int  has_ext_ci(const char *p, const char *ext);

static const char *basename_of(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

static void index_book(Db *db, int64_t lib_id, const Book *b) {
    sqlite3_exec(db->handle, "BEGIN;", NULL, NULL, NULL);
    for (size_t i = 0; i < b->count; i++) {
        int64_t page_id = 0;
        if (db_page_upsert(db, lib_id, (int)i, NULL, &page_id) != SQLITE_OK) continue;
        db_blob_add(db, page_id, 0, NULL, b->pages[i], NULL);
    }
    sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
}

static void index_manga(Db *db, int64_t lib_id, char **paths, size_t n) {
    sqlite3_exec(db->handle, "BEGIN;", NULL, NULL, NULL);
    for (size_t i = 0; i < n; i++) {
        db_page_upsert(db, lib_id, (int)i, paths[i], NULL);
    }
    sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
}

static void close_manga(AppContext *ctx) {
    for (size_t i = 0; i < 3; i++) {
        if (ctx->manga_cache[i].valid) UnloadTexture(ctx->manga_cache[i].tex);
        ctx->manga_cache[i].valid = 0;
    }
    for (size_t i = 0; i < ctx->image_count; i++) free(ctx->image_paths[i]);
    free(ctx->image_paths);
    ctx->image_paths = NULL;
    ctx->image_count = 0;
    ctx->image_cur = 0;
    ctx->has_manga = 0;
}

/* keep only cur-1..cur+1 loaded as textures */
static void manga_sync(AppContext *ctx) {
    size_t lo = ctx->image_cur > 0 ? ctx->image_cur - 1 : 0;
    size_t hi = ctx->image_cur + 1 < ctx->image_count ? ctx->image_cur + 1 : ctx->image_count - 1;
    for (size_t i = 0; i < 3; i++) {
        struct MangaSlot *s = &ctx->manga_cache[i];
        if (s->valid && (s->idx < lo || s->idx > hi)) {
            UnloadTexture(s->tex);
            s->valid = 0;
        }
    }
    for (size_t p = lo; p <= hi; p++) {
        int found = 0;
        for (size_t i = 0; i < 3; i++)
            if (ctx->manga_cache[i].valid && ctx->manga_cache[i].idx == p) { found = 1; break; }
        if (found) continue;
        for (size_t i = 0; i < 3; i++) {
            struct MangaSlot *s = &ctx->manga_cache[i];
            if (!s->valid) {
                s->tex = LoadTexture(ctx->image_paths[p]);
                s->idx = p;
                s->valid = 1;
                break;
            }
        }
    }
}

static const Texture2D *manga_current(const AppContext *ctx) {
    for (size_t i = 0; i < 3; i++)
        if (ctx->manga_cache[i].valid && ctx->manga_cache[i].idx == ctx->image_cur)
            return &ctx->manga_cache[i].tex;
    return NULL;
}

static void open_book(AppContext *ctx, const char *path) {
    if (!has_ext_ci(path, ".txt")) {
        TraceLog(LOG_WARNING, "open_book: only .txt supported, got %s", path);
        return;
    }
    ctx->book_load_attempted = 1;
    close_manga(ctx);
    if (ctx->book_loaded) { book_free(&ctx->book); ctx->book_loaded = 0; }
    ctx->cache_valid = 0;
    ctx->sub_page = 0;
    ctx->sub_count = 1;
    ctx->goto_last_sub = 0;
    if (book_load(&ctx->book, path)) {
        ctx->book_loaded = 1;
        int resume = db_library_get_progress(&ctx->db, path);
        db_library_upsert(&ctx->db, path, "book", basename_of(path), &ctx->lib_id);
        index_book(&ctx->db, ctx->lib_id, &ctx->book);
        /* single-chunk books persist the sub page, multi-chunk the chunk */
        if (resume > 0) {
            if (ctx->book.count == 1) ctx->sub_page = (size_t)resume;
            else if ((size_t)resume < ctx->book.count) ctx->book.cur = (size_t)resume;
        }
        snprintf(ctx->book_path, sizeof ctx->book_path, "%s", path);
        ctx->saved_index = (ctx->book.count == 1) ? ctx->sub_page : ctx->book.cur;
        if (ctx->panel_visible) { ctx->panel_visible = 0; ctx->is_animating = 1; }
        refresh_library(ctx);
    } else {
        book_free(&ctx->book);
    }
}

static int has_image_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return (strcmp(dot, ".png") == 0 || strcmp(dot, ".PNG") == 0 ||
            strcmp(dot, ".jpg") == 0 || strcmp(dot, ".JPG") == 0 ||
            strcmp(dot, ".jpeg") == 0 || strcmp(dot, ".JPEG") == 0);
}

static int path_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void open_image_folder(AppContext *ctx, const char *folder, const char *display_path) {
    ctx->book_load_attempted = 1;
    if (ctx->book_loaded) { book_free(&ctx->book); ctx->book_loaded = 0; }
    close_manga(ctx);

    FilePathList files = LoadDirectoryFiles(folder);
    char **selected = NULL;
    size_t n = 0;
    for (unsigned i = 0; i < files.count; i++) {
        if (has_image_ext(files.paths[i])) {
            char **g = realloc(selected, (n + 1) * sizeof *g);
            selected = g;
            size_t L = strlen(files.paths[i]);
            selected[n] = malloc(L + 1);
            memcpy(selected[n], files.paths[i], L + 1);
            n++;
        }
    }
    UnloadDirectoryFiles(files);
    if (n == 0) { free(selected); return; }

    qsort(selected, n, sizeof *selected, path_cmp);
    ctx->image_paths = selected;
    ctx->image_count = n;
    ctx->has_manga = 1;

    snprintf(ctx->book_path, sizeof ctx->book_path, "%s", display_path);
    int resume = db_library_get_progress(&ctx->db, display_path);
    db_library_upsert(&ctx->db, display_path, "manga", basename_of(display_path), &ctx->lib_id);
    index_manga(&ctx->db, ctx->lib_id, selected, n);
    if (resume > 0 && (size_t)resume < n) ctx->image_cur = (size_t)resume;
    ctx->saved_index = ctx->image_cur;

    if (ctx->panel_visible) { ctx->panel_visible = 0; ctx->is_animating = 1; }
    refresh_library(ctx);
}

static void open_manga(AppContext *ctx, const char *folder) {
    open_image_folder(ctx, folder, folder);
}

static int has_ext_ci(const char *p, const char *ext) {
    const char *d = strrchr(p, '.');
    return d && strcasecmp(d, ext) == 0;
}

static void refresh_library(AppContext *ctx) {
    ctx->lib_count = 0;
    sqlite3_stmt *s;
    if (db_library_list(&ctx->db, &s) != SQLITE_OK) return;
    while (sqlite3_step(s) == SQLITE_ROW) {
        if (ctx->lib_count == ctx->lib_cap) {
            size_t nc = ctx->lib_cap ? ctx->lib_cap * 2 : 16;
            struct LibEntry *g = realloc(ctx->lib_entries, nc * sizeof *g);
            if (!g) break;
            ctx->lib_entries = g;
            ctx->lib_cap = nc;
        }
        struct LibEntry *e = &ctx->lib_entries[ctx->lib_count++];
        e->id = sqlite3_column_int64(s, 0);
        const unsigned char *k = sqlite3_column_text(s, 1);
        const unsigned char *p = sqlite3_column_text(s, 2);
        const unsigned char *t = sqlite3_column_text(s, 3);
        snprintf(e->kind,  sizeof e->kind,  "%s", k ? (const char *)k : "");
        snprintf(e->path,  sizeof e->path,  "%s", p ? (const char *)p : "");
        snprintf(e->title, sizeof e->title, "%s",
                 (t && *t) ? (const char *)t : basename_of(e->path));
    }
    sqlite3_finalize(s);
}

/* extracted dirs are flat: files + .done, no subdirs */
static void remove_flat_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    char p[1024];
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        unlink(p);
    }
    closedir(d);
    rmdir(dir);
}

/* drop cache dirs whose archive is no longer in the library */
static void prune_cache_kind(const AppContext *ctx, const char *sub, const char *ext) {
    char root[1024];
    snprintf(root, sizeof root, "%scache/%s", GetApplicationDirectory(), sub);
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        int referenced = 0;
        for (size_t i = 0; i < ctx->lib_count; i++) {
            if (!has_ext_ci(ctx->lib_entries[i].path, ext)) continue;
            char h[17];
            au_hash_hex(ctx->lib_entries[i].path, h);
            if (strcmp(h, e->d_name) == 0) { referenced = 1; break; }
        }
        if (!referenced) {
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", root, e->d_name);
            remove_flat_dir(p);
        }
    }
    closedir(d);
}

static void prune_cache(const AppContext *ctx) {
    prune_cache_kind(ctx, "cbz", ".cbz");
    prune_cache_kind(ctx, "cbt", ".cbt");
}

static void open_archive(AppContext *ctx, const char *path,
                         int (*extract)(const char *, const char *, char *, size_t),
                         const char *kind) {
    char cache_root[1024];
    snprintf(cache_root, sizeof cache_root, "%scache", GetApplicationDirectory());
    char extracted[1024];
    if (extract(path, cache_root, extracted, sizeof extracted) != 0) {
        TraceLog(LOG_WARNING, "%s extract failed: %s", kind, path);
        ctx->book_load_attempted = 1;
        return;
    }
    open_image_folder(ctx, extracted, path);
}

static void open_library_entry(AppContext *ctx, const struct LibEntry *e) {
    if (strcmp(e->kind, "book") == 0) { open_book(ctx, e->path); return; }
    if (DirectoryExists(e->path)) open_manga(ctx, e->path);
    else if (has_ext_ci(e->path, ".cbz")) open_archive(ctx, e->path, cbz_extract, "cbz");
    else if (has_ext_ci(e->path, ".cbt")) open_archive(ctx, e->path, cbt_extract, "cbt");
}

static void app_next(AppContext *ctx) {
    if (ctx->book_loaded) {
        if (ctx->sub_page + 1 < ctx->sub_count) ctx->sub_page++;
        else if (ctx->book.cur + 1 < ctx->book.count) { book_next(&ctx->book); ctx->sub_page = 0; }
    } else if (ctx->has_manga && ctx->image_cur + 1 < ctx->image_count) ctx->image_cur++;
}

static void app_prev(AppContext *ctx) {
    if (ctx->book_loaded) {
        if (ctx->sub_page > 0) ctx->sub_page--;
        else if (ctx->book.cur > 0) { book_prev(&ctx->book); ctx->goto_last_sub = 1; }
    } else if (ctx->has_manga && ctx->image_cur > 0) ctx->image_cur--;
}

int main(void) {
    AppContext ctx = {0};
    ctx.screen_width = 800;
    ctx.screen_height = 600;
    ctx.panel_width = 200;
    ctx.panel_animation_speed = 10;
    ctx.panel_x = -ctx.panel_width;
    ctx.panel_toggle_button_width = 30;
    ctx.panel_toggle_button_height = 30;
    ctx.panel_toggle_button_offset = 5;
    strcpy(ctx.panel_toggle_icon, ">");
    ctx.panel_toggle_color = BLACK;
    ctx.panel_toggle_text_color = RAYWHITE;
    ctx.panel_content_button_offset = 5;
    ctx.panel_button_width = ctx.panel_width - 2 * ctx.panel_content_button_offset;
    ctx.panel_button_height = 40;
    ctx.open_button_y_offset = ctx.panel_toggle_button_offset + ctx.panel_toggle_button_height + 10;
    ctx.text_margin_horizontal = 30.0f;
    ctx.text_margin_top = 30.0f;
    ctx.text_margin_bottom = 80.0f;
    ctx.font_size = 20;
    ctx.text_line_spacing_factor = 1.2f;
    ctx.nav_button_width = 50.0f;
    ctx.nav_button_height = 50.0f;
    ctx.nav_button_margin = 20.0f;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(ctx.screen_width, ctx.screen_height, "GUANDAO - Book Reader");
    SetTargetFPS(60);
    ctx.font = load_app_font(ctx.font_size);

    char db_path[1024];
    snprintf(db_path, sizeof db_path, "%sguandao.db", GetApplicationDirectory());
    if (db_open(&ctx.db, db_path) != 0) {
        TraceLog(LOG_WARNING, "db_open failed: %s", db_path);
    }
    refresh_library(&ctx);
    prune_cache(&ctx);

    /* resume most recently opened entry */
    if (ctx.lib_count > 0) {
        const struct LibEntry *e = &ctx.lib_entries[0];
        if (DirectoryExists(e->path) || FileExists(e->path))
            open_library_entry(&ctx, e);
    }

    while (!WindowShouldClose() && !ctx.exit_request) {
        HandleInput(&ctx);
        UpdateState(&ctx);
        DrawUI(&ctx);
    }

    if (ctx.book_loaded) book_free(&ctx.book);
    close_manga(&ctx);
    free(ctx.lib_entries);
    free_lines(ctx.wrapped, ctx.wrapped_count);
    if (ctx.font.texture.id != GetFontDefault().texture.id) UnloadFont(ctx.font);
    db_close(&ctx.db);
    CloseWindow();
    return 0;
}

static void HandleInput(AppContext *ctx) {
    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        if (dropped.count > 0) {
            const char *p = dropped.paths[0];
            if (DirectoryExists(p)) open_manga(ctx, p);
            else if (has_ext_ci(p, ".cbz")) open_archive(ctx, p, cbz_extract, "cbz");
            else if (has_ext_ci(p, ".cbt")) open_archive(ctx, p, cbt_extract, "cbt");
            else open_book(ctx, p);
        }
        UnloadDroppedFiles(dropped);
    }

    if (ctx->is_animating) return;

    Vector2 mouse = GetMousePosition();
    int panel_clicked = 0;
    int left_clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    int right_clicked = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);

    if (left_clicked) {
        Rectangle toggle = {
            (float)ctx->panel_x + ctx->panel_width - ctx->panel_toggle_button_width - ctx->panel_toggle_button_offset,
            (float)ctx->panel_toggle_button_offset,
            (float)ctx->panel_toggle_button_width,
            (float)ctx->panel_toggle_button_height,
        };
        if (ctx->panel_x <= -ctx->panel_width) toggle.x = ctx->panel_toggle_button_offset;

        if (CheckCollisionPointRec(mouse, toggle)) {
            ctx->panel_visible = !ctx->panel_visible;
            ctx->is_animating = 1;
            panel_clicked = 1;
        }

        if (ctx->panel_x > -ctx->panel_width + 5) {
            Rectangle open_btn = {
                (float)ctx->panel_x + ctx->panel_content_button_offset,
                (float)ctx->open_button_y_offset,
                (float)ctx->panel_button_width,
                (float)ctx->panel_button_height,
            };
            if (CheckCollisionPointRec(mouse, open_btn)) {
                panel_clicked = 1;
                const char *filters[] = {"*.txt"};
                const char *file = tinyfd_openFileDialog("Select Book (.txt)", "", 1, filters, "Text Files", 0);

                if (file) open_book(ctx, file);
            }

            Rectangle manga_btn = {
                (float)ctx->panel_x + ctx->panel_content_button_offset,
                (float)ctx->open_button_y_offset + ctx->panel_button_height + 5,
                (float)ctx->panel_button_width,
                (float)ctx->panel_button_height,
            };
            if (CheckCollisionPointRec(mouse, manga_btn)) {
                panel_clicked = 1;
                const char *folder = tinyfd_selectFolderDialog("Select Manga Folder", "");
                if (folder) open_manga(ctx, folder);
            }

            Rectangle exit_btn = {
                (float)ctx->panel_x + ctx->panel_content_button_offset,
                (float)ctx->screen_height - ctx->panel_button_height - ctx->panel_content_button_offset,
                (float)ctx->panel_button_width,
                (float)ctx->panel_button_height,
            };
            if (CheckCollisionPointRec(mouse, exit_btn)) {
                panel_clicked = 1;
                ctx->exit_request = 1;
            }

        }

    }

    if ((left_clicked || right_clicked) && ctx->panel_x > -ctx->panel_width + 5) {
        int lib_y0 = ctx->open_button_y_offset + 2 * ctx->panel_button_height + 25;
        int lib_y1 = (int)ctx->screen_height - ctx->panel_button_height - ctx->panel_content_button_offset - 10;
        int row_h = 28;
        for (size_t i = 0; i < ctx->lib_count; i++) {
            int y = lib_y0 + (int)i * row_h - (int)ctx->lib_scroll;
            if (y + row_h < lib_y0 || y > lib_y1) continue;
            Rectangle r = {
                (float)ctx->panel_x + ctx->panel_content_button_offset,
                (float)y,
                (float)ctx->panel_button_width,
                (float)(row_h - 4),
            };
            if (!CheckCollisionPointRec(mouse, r)) continue;
            if (right_clicked) {
                db_library_delete(&ctx->db, ctx->lib_entries[i].id);
                refresh_library(ctx);
                prune_cache(ctx);
            } else {
                open_library_entry(ctx, &ctx->lib_entries[i]);
            }
            panel_clicked = 1;
            break;
        }
    }

    if (left_clicked && (ctx->book_loaded || ctx->has_manga) && !panel_clicked) {
        if (CheckCollisionPointRec(mouse, ctx->next_page_button_rect)) app_next(ctx);
        else if (CheckCollisionPointRec(mouse, ctx->prev_page_button_rect)) app_prev(ctx);
    }

    if (ctx->book_loaded || ctx->has_manga) {
        if (IsKeyPressed(KEY_RIGHT)) app_next(ctx);
        if (IsKeyPressed(KEY_LEFT))  app_prev(ctx);
    }

    if (ctx->panel_x > -ctx->panel_width + 5 && ctx->lib_count > 0) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0 && mouse.x < ctx->panel_x + ctx->panel_width) {
            ctx->lib_scroll -= wheel * 28;
            if (ctx->lib_scroll < 0) ctx->lib_scroll = 0;
        }
    }
}

static void UpdateState(AppContext *ctx) {
    int new_w = GetScreenWidth();
    int new_h = GetScreenHeight();
    if (new_w != ctx->screen_width || new_h != ctx->screen_height) {
        ctx->screen_width = new_w;
        ctx->screen_height = new_h;
        ctx->panel_button_width = ctx->panel_width - 2 * ctx->panel_content_button_offset;
        ctx->cache_valid = 0;
    }

    if (ctx->is_animating) {
        if (ctx->panel_visible) {
            ctx->panel_x += ctx->panel_animation_speed;
            if (ctx->panel_x >= 0) { ctx->panel_x = 0; ctx->is_animating = 0; }
        } else {
            ctx->panel_x -= ctx->panel_animation_speed;
            if (ctx->panel_x <= -ctx->panel_width) { ctx->panel_x = -ctx->panel_width; ctx->is_animating = 0; }
        }
    }

    strcpy(ctx->panel_toggle_icon, ctx->panel_visible ? "<" : ">");
    ctx->panel_toggle_color      = ctx->panel_visible ? RAYWHITE : BLACK;
    ctx->panel_toggle_text_color = ctx->panel_visible ? BLACK : RAYWHITE;

    float panel_offset = (ctx->panel_x > -ctx->panel_width) ? (ctx->panel_width + ctx->panel_x) : 0;
    ctx->text_display_area = (Rectangle){
        ctx->text_margin_horizontal + panel_offset,
        ctx->text_margin_top,
        ctx->screen_width - (ctx->text_margin_horizontal + panel_offset) - ctx->text_margin_horizontal,
        ctx->screen_height - ctx->text_margin_top - ctx->text_margin_bottom,
    };
    if (ctx->text_display_area.width < 10) ctx->text_display_area.width = 10;

    float lh = ctx->font_size * ctx->text_line_spacing_factor;
    ctx->lines_per_page = 1;
    if (ctx->text_display_area.height > ctx->font_size)
        ctx->lines_per_page = (size_t)((ctx->text_display_area.height - ctx->font_size) / lh) + 1;

    if (ctx->has_manga) {
        manga_sync(ctx);
        if (ctx->image_cur != ctx->saved_index) {
            db_library_set_progress(&ctx->db, ctx->lib_id, (int)ctx->image_cur);
            ctx->saved_index = ctx->image_cur;
        }
        snprintf(ctx->page_info, sizeof ctx->page_info, "Page %zu / %zu", ctx->image_cur + 1, ctx->image_count);
    } else if (ctx->book_loaded) {
        size_t idx = book_index(&ctx->book);
        if (!ctx->cache_valid || ctx->cached_index != idx) {
            free_lines(ctx->wrapped, ctx->wrapped_count);
            ctx->wrapped = wrap_text(ctx->font, book_page(&ctx->book),
                                     (float)ctx->font_size,
                                     ctx->screen_width - 2 * ctx->text_margin_horizontal,
                                     &ctx->wrapped_count);
            ctx->cached_index = idx;
            ctx->cache_valid = 1;
        }

        ctx->sub_count = ctx->wrapped_count
            ? (ctx->wrapped_count + ctx->lines_per_page - 1) / ctx->lines_per_page
            : 1;
        if (ctx->goto_last_sub) { ctx->sub_page = ctx->sub_count - 1; ctx->goto_last_sub = 0; }
        if (ctx->sub_page >= ctx->sub_count) ctx->sub_page = ctx->sub_count - 1;

        size_t pos = (ctx->book.count == 1) ? ctx->sub_page : ctx->book.cur;
        if (pos != ctx->saved_index) {
            db_library_set_progress(&ctx->db, ctx->lib_id, (int)pos);
            ctx->saved_index = pos;
        }

        if (ctx->book.count == 1)
            snprintf(ctx->page_info, sizeof ctx->page_info, "Page %zu / %zu",
                     ctx->sub_page + 1, ctx->sub_count);
        else if (ctx->sub_count > 1)
            snprintf(ctx->page_info, sizeof ctx->page_info, "Page %zu.%zu / %zu",
                     idx, ctx->sub_page + 1, book_count(&ctx->book));
        else
            snprintf(ctx->page_info, sizeof ctx->page_info, "Page %zu / %zu",
                     idx, book_count(&ctx->book));
    }

    ctx->prev_page_button_rect = (Rectangle){
        ctx->nav_button_margin,
        ctx->screen_height - ctx->nav_button_height - ctx->nav_button_margin,
        ctx->nav_button_width, ctx->nav_button_height,
    };
    ctx->next_page_button_rect = (Rectangle){
        ctx->screen_width - ctx->nav_button_width - ctx->nav_button_margin,
        ctx->screen_height - ctx->nav_button_height - ctx->nav_button_margin,
        ctx->nav_button_width, ctx->nav_button_height,
    };
}

static void DrawUI(const AppContext *ctx) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    if (!ctx->book_load_attempted) {
        const char *title = "GUANDAO";
        int ts = 40;
        int tw = MeasureText(title, ts);
        DrawText(title, (ctx->screen_width - tw) / 2, ctx->screen_height / 2 - ts / 2, ts, LIGHTGRAY);

        const char *hint = "Use Panel '>' to Open Book (.txt)";
        int hs = 18;
        int hw = MeasureText(hint, hs);
        DrawText(hint, (ctx->screen_width - hw) / 2, ctx->screen_height / 2 + ts / 2 + 10, hs, GRAY);
    } else if (ctx->has_manga) {
        const Texture2D *t = manga_current(ctx);
        if (t) {
            float aw = ctx->text_display_area.width;
            float ah = ctx->text_display_area.height;
            float sx = aw / t->width;
            float sy = ah / t->height;
            float s = sx < sy ? sx : sy;
            float dw = t->width * s;
            float dh = t->height * s;
            float dx = ctx->text_display_area.x + (aw - dw) / 2;
            float dy = ctx->text_display_area.y + (ah - dh) / 2;
            DrawTextureEx(*t, (Vector2){dx, dy}, 0, s, WHITE);
        }

        int piw = MeasureText(ctx->page_info, ctx->font_size);
        DrawText(ctx->page_info, (ctx->screen_width - piw) / 2,
                 ctx->screen_height - ctx->nav_button_margin - ctx->nav_button_height / 2,
                 ctx->font_size, DARKGRAY);

        int can_prev = (ctx->image_cur > 0);
        DrawRectangleRec(ctx->prev_page_button_rect, can_prev ? LIGHTGRAY : DARKGRAY);
        DrawText("<", ctx->prev_page_button_rect.x + ctx->nav_button_width / 2 - MeasureText("<", 30) / 2,
                 ctx->prev_page_button_rect.y + ctx->nav_button_height / 2 - 15, 30, can_prev ? BLACK : GRAY);

        int can_next = (ctx->image_cur + 1 < ctx->image_count);
        DrawRectangleRec(ctx->next_page_button_rect, can_next ? LIGHTGRAY : DARKGRAY);
        DrawText(">", ctx->next_page_button_rect.x + ctx->nav_button_width / 2 - MeasureText(">", 30) / 2,
                 ctx->next_page_button_rect.y + ctx->nav_button_height / 2 - 15, 30, can_next ? BLACK : GRAY);
    } else if (ctx->book_loaded) {
        float y = ctx->text_display_area.y;
        float lh = ctx->font_size * ctx->text_line_spacing_factor;
        size_t first = ctx->sub_page * ctx->lines_per_page;
        size_t last = first + ctx->lines_per_page;
        if (last > ctx->wrapped_count) last = ctx->wrapped_count;
        for (size_t i = first; i < last; i++) {
            if (y + ctx->font_size > ctx->text_display_area.y + ctx->text_display_area.height) break;
            DrawTextEx(ctx->font, ctx->wrapped[i],
                       (Vector2){ctx->text_display_area.x, y},
                       (float)ctx->font_size, 1.0f, BLACK);
            y += lh;
        }

        int piw = MeasureText(ctx->page_info, ctx->font_size);
        DrawText(ctx->page_info, (ctx->screen_width - piw) / 2,
                 ctx->screen_height - ctx->nav_button_margin - ctx->nav_button_height / 2,
                 ctx->font_size, DARKGRAY);

        int can_prev = (ctx->sub_page > 0 || book_index(&ctx->book) > 1);
        DrawRectangleRec(ctx->prev_page_button_rect, can_prev ? LIGHTGRAY : DARKGRAY);
        DrawText("<", ctx->prev_page_button_rect.x + ctx->nav_button_width / 2 - MeasureText("<", 30) / 2,
                 ctx->prev_page_button_rect.y + ctx->nav_button_height / 2 - 15, 30, can_prev ? BLACK : GRAY);

        int can_next = (ctx->sub_page + 1 < ctx->sub_count || book_index(&ctx->book) < book_count(&ctx->book));
        DrawRectangleRec(ctx->next_page_button_rect, can_next ? LIGHTGRAY : DARKGRAY);
        DrawText(">", ctx->next_page_button_rect.x + ctx->nav_button_width / 2 - MeasureText(">", 30) / 2,
                 ctx->next_page_button_rect.y + ctx->nav_button_height / 2 - 15, 30, can_next ? BLACK : GRAY);
    } else {
        const char *err = "Failed to load book.";
        int ew = MeasureText(err, 20);
        DrawText(err, (ctx->screen_width - ew) / 2, ctx->screen_height / 2 - 10, 20, MAROON);
    }

    DrawRectangle(ctx->panel_x, 0, ctx->panel_width, ctx->screen_height, BLACK);

    int toggle_x = (ctx->panel_x <= -ctx->panel_width)
        ? ctx->panel_toggle_button_offset
        : ctx->panel_x + ctx->panel_width - ctx->panel_toggle_button_width - ctx->panel_toggle_button_offset;
    DrawRectangle(toggle_x, ctx->panel_toggle_button_offset,
                  ctx->panel_toggle_button_width, ctx->panel_toggle_button_height, ctx->panel_toggle_color);
    DrawText(ctx->panel_toggle_icon, toggle_x + 10, ctx->panel_toggle_button_offset + 5, 20, ctx->panel_toggle_text_color);

    if (ctx->panel_x > -ctx->panel_width + 5) {
        int x = ctx->panel_x + ctx->panel_content_button_offset;
        DrawRectangle(x, ctx->open_button_y_offset, ctx->panel_button_width, ctx->panel_button_height, DARKGRAY);
        DrawText("Open Book (.txt)", x + (ctx->panel_button_width - MeasureText("Open Book (.txt)", 20)) / 2,
                 ctx->open_button_y_offset + 10, 20, WHITE);

        int my = ctx->open_button_y_offset + ctx->panel_button_height + 5;
        DrawRectangle(x, my, ctx->panel_button_width, ctx->panel_button_height, DARKGRAY);
        DrawText("Open Manga", x + (ctx->panel_button_width - MeasureText("Open Manga", 20)) / 2, my + 10, 20, WHITE);

        int ey = ctx->screen_height - ctx->panel_button_height - ctx->panel_content_button_offset;
        DrawRectangle(x, ey, ctx->panel_button_width, ctx->panel_button_height, RED);
        DrawText("EXIT", x + (ctx->panel_button_width - MeasureText("EXIT", 20)) / 2, ey + 10, 20, WHITE);

        int lib_y0 = ctx->open_button_y_offset + 2 * ctx->panel_button_height + 25;
        int lib_y1 = ey - 10;
        DrawText("Library", x, lib_y0 - 18, 14, LIGHTGRAY);
        BeginScissorMode(x, lib_y0, ctx->panel_button_width, lib_y1 - lib_y0);
        int row_h = 28;
        for (size_t i = 0; i < ctx->lib_count; i++) {
            int y = lib_y0 + (int)i * row_h - (int)ctx->lib_scroll;
            if (y + row_h < lib_y0 || y > lib_y1) continue;
            Color bg = (strcmp(ctx->lib_entries[i].kind, "book") == 0)
                ? (Color){40, 40, 60, 255}
                : (Color){40, 60, 40, 255};
            DrawRectangle(x, y, ctx->panel_button_width, row_h - 4, bg);
            DrawTextEx(ctx->font, ctx->lib_entries[i].title,
                       (Vector2){x + 6, y + 4}, 14, 1, WHITE);
        }
        EndScissorMode();
    }

    EndDrawing();
}
