#include "raylib.h"
#include "tinyfiledialogs.h"
#include "book_manager.h"
#include "db.h"
#include "cbz.h"
#include "cbt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>

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
    char page_info[64];

    Texture2D *images;
    size_t image_count;
    size_t image_cur;
    int has_manga;

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
    for (size_t i = 0; i < ctx->image_count; i++) UnloadTexture(ctx->images[i]);
    free(ctx->images);
    ctx->images = NULL;
    ctx->image_count = 0;
    ctx->image_cur = 0;
    ctx->has_manga = 0;
}

static void open_book(AppContext *ctx, const char *path) {
    ctx->book_load_attempted = 1;
    close_manga(ctx);
    if (ctx->book_loaded) { book_free(&ctx->book); ctx->book_loaded = 0; }
    ctx->cache_valid = 0;
    if (book_load(&ctx->book, path)) {
        ctx->book_loaded = 1;
        int resume = db_library_get_progress(&ctx->db, path);
        db_library_upsert(&ctx->db, path, "book", basename_of(path), &ctx->lib_id);
        index_book(&ctx->db, ctx->lib_id, &ctx->book);
        if (resume > 0 && (size_t)resume < ctx->book.count) ctx->book.cur = (size_t)resume;
        snprintf(ctx->book_path, sizeof ctx->book_path, "%s", path);
        ctx->saved_index = ctx->book.cur;
        if (ctx->panel_visible) { ctx->panel_visible = 0; ctx->is_animating = 1; }
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
    ctx->images = malloc(n * sizeof *ctx->images);
    for (size_t i = 0; i < n; i++) {
        ctx->images[i] = LoadTexture(selected[i]);
    }
    ctx->image_count = n;
    ctx->has_manga = 1;

    snprintf(ctx->book_path, sizeof ctx->book_path, "%s", display_path);
    int resume = db_library_get_progress(&ctx->db, display_path);
    db_library_upsert(&ctx->db, display_path, "manga", basename_of(display_path), &ctx->lib_id);
    index_manga(&ctx->db, ctx->lib_id, selected, n);
    for (size_t i = 0; i < n; i++) free(selected[i]);
    free(selected);
    if (resume > 0 && (size_t)resume < n) ctx->image_cur = (size_t)resume;
    ctx->saved_index = ctx->image_cur;

    if (ctx->panel_visible) { ctx->panel_visible = 0; ctx->is_animating = 1; }
}

static void open_manga(AppContext *ctx, const char *folder) {
    open_image_folder(ctx, folder, folder);
}

static int has_ext_ci(const char *p, const char *ext) {
    const char *d = strrchr(p, '.');
    return d && strcasecmp(d, ext) == 0;
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

static void app_next(AppContext *ctx) {
    if (ctx->book_loaded) book_next(&ctx->book);
    else if (ctx->has_manga && ctx->image_cur + 1 < ctx->image_count) ctx->image_cur++;
}

static void app_prev(AppContext *ctx) {
    if (ctx->book_loaded) book_prev(&ctx->book);
    else if (ctx->has_manga && ctx->image_cur > 0) ctx->image_cur--;
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

    while (!WindowShouldClose() && !ctx.exit_request) {
        HandleInput(&ctx);
        UpdateState(&ctx);
        DrawUI(&ctx);
    }

    if (ctx.book_loaded) book_free(&ctx.book);
    close_manga(&ctx);
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

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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

        if ((ctx->book_loaded || ctx->has_manga) && !panel_clicked) {
            if (CheckCollisionPointRec(mouse, ctx->next_page_button_rect)) app_next(ctx);
            else if (CheckCollisionPointRec(mouse, ctx->prev_page_button_rect)) app_prev(ctx);
        }
    }

    if (ctx->book_loaded || ctx->has_manga) {
        if (IsKeyPressed(KEY_RIGHT)) app_next(ctx);
        if (IsKeyPressed(KEY_LEFT))  app_prev(ctx);
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

    if (ctx->has_manga) {
        if (ctx->image_cur != ctx->saved_index) {
            db_library_set_progress(&ctx->db, ctx->lib_id, (int)ctx->image_cur);
            ctx->saved_index = ctx->image_cur;
        }
        snprintf(ctx->page_info, sizeof ctx->page_info, "Page %zu / %zu", ctx->image_cur + 1, ctx->image_count);
    } else if (ctx->book_loaded) {
        size_t idx = book_index(&ctx->book);
        if (ctx->book.cur != ctx->saved_index) {
            db_library_set_progress(&ctx->db, ctx->lib_id, (int)ctx->book.cur);
            ctx->saved_index = ctx->book.cur;
        }
        snprintf(ctx->page_info, sizeof ctx->page_info, "Page %zu / %zu", idx, book_count(&ctx->book));
        if (!ctx->cache_valid || ctx->cached_index != idx) {
            free_lines(ctx->wrapped, ctx->wrapped_count);
            ctx->wrapped = wrap_text(ctx->font, book_page(&ctx->book),
                                     (float)ctx->font_size,
                                     ctx->screen_width - 2 * ctx->text_margin_horizontal,
                                     &ctx->wrapped_count);
            ctx->cached_index = idx;
            ctx->cache_valid = 1;
        }
    }

    float panel_offset = (ctx->panel_x > -ctx->panel_width) ? (ctx->panel_width + ctx->panel_x) : 0;
    ctx->text_display_area = (Rectangle){
        ctx->text_margin_horizontal + panel_offset,
        ctx->text_margin_top,
        ctx->screen_width - (ctx->text_margin_horizontal + panel_offset) - ctx->text_margin_horizontal,
        ctx->screen_height - ctx->text_margin_top - ctx->text_margin_bottom,
    };
    if (ctx->text_display_area.width < 10) ctx->text_display_area.width = 10;

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
        Texture2D t = ctx->images[ctx->image_cur];
        float aw = ctx->text_display_area.width;
        float ah = ctx->text_display_area.height;
        float sx = aw / t.width;
        float sy = ah / t.height;
        float s = sx < sy ? sx : sy;
        float dw = t.width * s;
        float dh = t.height * s;
        float dx = ctx->text_display_area.x + (aw - dw) / 2;
        float dy = ctx->text_display_area.y + (ah - dh) / 2;
        DrawTextureEx(t, (Vector2){dx, dy}, 0, s, WHITE);

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
        for (size_t i = 0; i < ctx->wrapped_count; i++) {
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

        int can_prev = (book_index(&ctx->book) > 1);
        DrawRectangleRec(ctx->prev_page_button_rect, can_prev ? LIGHTGRAY : DARKGRAY);
        DrawText("<", ctx->prev_page_button_rect.x + ctx->nav_button_width / 2 - MeasureText("<", 30) / 2,
                 ctx->prev_page_button_rect.y + ctx->nav_button_height / 2 - 15, 30, can_prev ? BLACK : GRAY);

        int can_next = (book_index(&ctx->book) < book_count(&ctx->book));
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
    }

    EndDrawing();
}
