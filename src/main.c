#include "raylib.h"
#include "tinyfiledialogs.h"
#include "book_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    char page_info[64];

    size_t cached_index;
    int cache_valid;
    char **wrapped;
    size_t wrapped_count;

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

    InitWindow(ctx.screen_width, ctx.screen_height, "GUANDAO - Book Reader");
    SetTargetFPS(60);

    while (!WindowShouldClose() && !ctx.exit_request) {
        HandleInput(&ctx);
        UpdateState(&ctx);
        DrawUI(&ctx);
    }

    if (ctx.book_loaded) book_free(&ctx.book);
    free_lines(ctx.wrapped, ctx.wrapped_count);
    CloseWindow();
    return 0;
}

static void HandleInput(AppContext *ctx) {
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

                if (file) {
                    ctx->book_load_attempted = 1;
                    if (ctx->book_loaded) { book_free(&ctx->book); ctx->book_loaded = 0; }
                    ctx->cache_valid = 0;

                    if (book_load(&ctx->book, file)) {
                        ctx->book_loaded = 1;
                        if (ctx->panel_visible) {
                            ctx->panel_visible = 0;
                            ctx->is_animating = 1;
                        }
                    } else {
                        book_free(&ctx->book);
                    }
                }
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

        if (ctx->book_loaded && !panel_clicked) {
            if (CheckCollisionPointRec(mouse, ctx->next_page_button_rect)) book_next(&ctx->book);
            else if (CheckCollisionPointRec(mouse, ctx->prev_page_button_rect)) book_prev(&ctx->book);
        }
    }

    if (ctx->book_loaded) {
        if (IsKeyPressed(KEY_RIGHT)) book_next(&ctx->book);
        if (IsKeyPressed(KEY_LEFT))  book_prev(&ctx->book);
    }
}

static void UpdateState(AppContext *ctx) {
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

    if (ctx->book_loaded) {
        size_t idx = book_index(&ctx->book);
        snprintf(ctx->page_info, sizeof ctx->page_info, "Page %zu / %zu", idx, book_count(&ctx->book));
        if (!ctx->cache_valid || ctx->cached_index != idx) {
            free_lines(ctx->wrapped, ctx->wrapped_count);
            ctx->wrapped = wrap_text(GetFontDefault(), book_page(&ctx->book),
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
    } else if (ctx->book_loaded) {
        float y = ctx->text_display_area.y;
        float lh = ctx->font_size * ctx->text_line_spacing_factor;
        for (size_t i = 0; i < ctx->wrapped_count; i++) {
            if (y + ctx->font_size > ctx->text_display_area.y + ctx->text_display_area.height) break;
            DrawTextEx(GetFontDefault(), ctx->wrapped[i],
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

        int ey = ctx->screen_height - ctx->panel_button_height - ctx->panel_content_button_offset;
        DrawRectangle(x, ey, ctx->panel_button_width, ctx->panel_button_height, RED);
        DrawText("EXIT", x + (ctx->panel_button_width - MeasureText("EXIT", 20)) / 2, ey + 10, 20, WHITE);
    }

    EndDrawing();
}
