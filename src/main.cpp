#include "raylib.h"
#include "tinyfiledialogs.h"
#include "book_manager.h"
#include "utils.h"
#include <string>
#include <vector>
#include <memory>   // smart pointer support
#include <sstream>  // WrapTextLines helper

// one‑shot wrapper that pre‑computes word‑wrapped lines for a page
static std::vector<std::string> WrapTextLines(Font font,
                                             const std::string& text,
                                             float fontSize,
                                             float maxWidth) {
    std::vector<std::string> lines;
    std::istringstream ss(text);
    std::string word, current;
    while (ss >> word) {
        bool forcedBreak = false;
        size_t nlPos = word.find('\n');
        if (nlPos != std::string::npos) {
            forcedBreak = true;
            word = word.substr(0, nlPos);
        }
        std::string candidate = (current.empty() ? "" : " ") + word;
        if (MeasureTextEx(font, (current + candidate).c_str(), fontSize, 1).x <= maxWidth) {
            current += candidate;
        } else {
            if (!current.empty()) lines.push_back(current);
            current = word; // word starts new line
        }
        if (forcedBreak) {
            lines.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

// holds all the important variables for the app
struct AppContext {
    int screen_width = 800;
    int screen_height = 600;

    const int panel_width = 200;
    const int panel_animation_speed = 10;
    int panel_x = -panel_width;
    bool panel_visible = false;
    bool is_animating = false;

    const int panel_toggle_button_width = 30;
    const int panel_toggle_button_height = 30;
    const int panel_toggle_button_offset = 5;
    std::string panel_toggle_icon = ">";
    Color panel_toggle_color = BLACK;
    Color panel_toggle_text_color = RAYWHITE;

    const int panel_content_button_offset = 5;
    const int panel_button_width = panel_width - 2 * panel_content_button_offset;
    const int panel_button_height = 40;
    const int open_button_y_offset = panel_toggle_button_offset + panel_toggle_button_height + 10;

    std::unique_ptr<Book> currentBook; // managed automatically
    std::string currentPageText = "";
    std::string pageInfoText = "";
    int currentPageNum = 0;
    int totalPages = 0;
    bool bookLoadAttemptedOrSuccess = false;

    // cached layout so we don't re‑measure every frame
    std::string cachedPageText;
    std::vector<std::string> cachedLines;

    bool exitRequest = false; // a flag to tell the main loop to exit

    const float textMarginHorizontal = 30.0f;
    const float textMarginTop = 30.0f;
    const float textMarginBottom = 80.0f;
    const int fontSize = 20;
    const float textLineSpacingFactor = 1.2f;
    Rectangle textDisplayArea = {0};

    const float navButtonWidth = 50.0f;
    const float navButtonHeight = 50.0f;
    const float navButtonMargin = 20.0f;
    Rectangle prevPageButtonRect = {0};
    Rectangle nextPageButtonRect = {0};
};

// checks for mouse clicks and key presses
void HandleInput(AppContext& ctx);

// moves things around, like the panel animation
void UpdateState(AppContext& ctx);

// puts everything on the screen
void DrawUI(const AppContext& ctx);

int main(void) {
    AppContext ctx;

    InitWindow(ctx.screen_width, ctx.screen_height, "GUANDAO - Book Reader");
    SetTargetFPS(60);

    while (!WindowShouldClose() && !ctx.exitRequest) {
        HandleInput(ctx);
        UpdateState(ctx);
        DrawUI(ctx);
    }

    // currentBook automatically cleaned by unique_ptr
    CloseWindow();
    return 0;
}

void HandleInput(AppContext& ctx) {
    // block interaction while the panel is moving
    if (ctx.is_animating) return;

    Vector2 mouse_position = GetMousePosition();
    bool panelWasClicked = false;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Rectangle panel_toggle_rect = {
            (float)ctx.panel_x + ctx.panel_width - ctx.panel_toggle_button_width - ctx.panel_toggle_button_offset,
            (float)ctx.panel_toggle_button_offset,
            (float)ctx.panel_toggle_button_width,
            (float)ctx.panel_toggle_button_height
        };
        if (ctx.panel_x <= -ctx.panel_width) {
            panel_toggle_rect.x = ctx.panel_toggle_button_offset;
        }

        if (CheckCollisionPointRec(mouse_position, panel_toggle_rect)) {
            ctx.panel_visible = !ctx.panel_visible;
            ctx.is_animating = true;
            panelWasClicked = true;
        }

        if (ctx.panel_x > -ctx.panel_width + 5) {
            Rectangle open_button_rect = {(float)ctx.panel_x + ctx.panel_content_button_offset, (float)ctx.open_button_y_offset, (float)ctx.panel_button_width, (float)ctx.panel_button_height};
            if (CheckCollisionPointRec(mouse_position, open_button_rect)) {
                panelWasClicked = true;
                const char *filters[] = {"*.txt"};
                const char *file = tinyfd_openFileDialog("Select Book (.txt)", "", 1, filters, "Text Files", 0);

                if (file) {
                    ctx.bookLoadAttemptedOrSuccess = true;
                    ctx.currentBook.reset(); // release old book
                    ctx.cachedPageText.clear(); // invalidate cache
                    ctx.cachedLines.clear();

                    auto candidate = std::make_unique<Book>(file); // unique_ptr construction
                    if (candidate && candidate->isValid()) {
                        ctx.currentBook = std::move(candidate);
                        if (ctx.panel_visible) {
                            ctx.panel_visible = false;
                            ctx.is_animating = true;
                        }
                    } else {
                        ctx.currentPageText = "Error loading book:\n" + std::string(file) + "\n\nPlease ensure the file exists, is readable,\nand contains '<PAGE_BREAK>' markers.";
                        ctx.pageInfoText = "Book Error";
                        ctx.currentPageNum = 0;
                        ctx.totalPages = 0;
                    }
                }
            }

            Rectangle exit_button_rect = {(float)ctx.panel_x + ctx.panel_content_button_offset, (float)ctx.screen_height - ctx.panel_button_height - ctx.panel_content_button_offset, (float)ctx.panel_button_width, (float)ctx.panel_button_height};
            if (CheckCollisionPointRec(mouse_position, exit_button_rect)) {
                panelWasClicked = true;
                ctx.exitRequest = true;
            }
        }

        if (ctx.currentBook && !panelWasClicked) {
            if (CheckCollisionPointRec(mouse_position, ctx.nextPageButtonRect) && ctx.currentPageNum < ctx.totalPages) {
                ctx.currentBook->nextPage();
            } else if (CheckCollisionPointRec(mouse_position, ctx.prevPageButtonRect) && ctx.currentPageNum > 1) {
                ctx.currentBook->previousPage();
            }
        }
    }

    if (ctx.currentBook) {
        if (IsKeyPressed(KEY_RIGHT) && ctx.currentPageNum < ctx.totalPages) {
            ctx.currentBook->nextPage();
        }
        if (IsKeyPressed(KEY_LEFT) && ctx.currentPageNum > 1) {
            ctx.currentBook->previousPage();
        }
    }
}

void UpdateState(AppContext& ctx) {
    if (ctx.is_animating) {
        if (ctx.panel_visible) {
            ctx.panel_x += ctx.panel_animation_speed;
            if (ctx.panel_x >= 0) {
                ctx.panel_x = 0;
                ctx.is_animating = false;
            }
        } else {
            ctx.panel_x -= ctx.panel_animation_speed;
            if (ctx.panel_x <= -ctx.panel_width) {
                ctx.panel_x = -ctx.panel_width;
                ctx.is_animating = false;
            }
        }
    }

    ctx.panel_toggle_icon = ctx.panel_visible ? "<" : ">";
    ctx.panel_toggle_color = ctx.panel_visible ? RAYWHITE : BLACK;
    ctx.panel_toggle_text_color = ctx.panel_visible ? BLACK : RAYWHITE;

    if (ctx.currentBook) {
        ctx.currentPageNum = ctx.currentBook->getCurrentPageNumber();
        ctx.totalPages = ctx.currentBook->getLength();
        ctx.currentPageText = ctx.currentBook->getCurrentPage();
        ctx.pageInfoText = "Page " + std::to_string(ctx.currentPageNum) + " / " + std::to_string(ctx.totalPages);
    }

    // refresh wrapped cache only if text has changed
    if (ctx.currentPageText != ctx.cachedPageText) {
        ctx.cachedPageText = ctx.currentPageText;
        ctx.cachedLines = WrapTextLines(GetFontDefault(), ctx.cachedPageText, (float)ctx.fontSize, ctx.screen_width - 2 * ctx.textMarginHorizontal);
    }

    float currentPanelOffset = (ctx.panel_x > -ctx.panel_width) ? (ctx.panel_width + ctx.panel_x) : 0;
    ctx.textDisplayArea = {
        ctx.textMarginHorizontal + currentPanelOffset,
        ctx.textMarginTop,
        ctx.screen_width - (ctx.textMarginHorizontal + currentPanelOffset) - ctx.textMarginHorizontal,
        ctx.screen_height - ctx.textMarginTop - ctx.textMarginBottom
    };
    if (ctx.textDisplayArea.width < 10) ctx.textDisplayArea.width = 10;

    ctx.prevPageButtonRect = { ctx.navButtonMargin, ctx.screen_height - ctx.navButtonHeight - ctx.navButtonMargin, ctx.navButtonWidth, ctx.navButtonHeight };
    ctx.nextPageButtonRect = { ctx.screen_width - ctx.navButtonWidth - ctx.navButtonMargin, ctx.screen_height - ctx.navButtonHeight - ctx.navButtonMargin, ctx.navButtonWidth, ctx.navButtonHeight };
}

void DrawUI(const AppContext& ctx) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    if (!ctx.bookLoadAttemptedOrSuccess) {
        const char* previewText = "GUANDAO";
        int previewFontSize = 40;
        int textWidth = MeasureText(previewText, previewFontSize);
        DrawText(previewText, (ctx.screen_width - textWidth) / 2, ctx.screen_height / 2 - previewFontSize / 2, previewFontSize, LIGHTGRAY);

        const char* instructionText = "Use Panel '>' to Open Book (.txt)";
        int instructionFontSize = 18;
        int instructionWidth = MeasureText(instructionText, instructionFontSize);
        DrawText(instructionText, (ctx.screen_width - instructionWidth) / 2, ctx.screen_height / 2 + previewFontSize / 2 + 10, instructionFontSize, GRAY);
    } else {
        // draw from cached lines (no per‑frame MeasureTextEx)
        float y = ctx.textDisplayArea.y;
        float lineHeight = ctx.fontSize * ctx.textLineSpacingFactor;
        for (const std::string& ln : ctx.cachedLines) {
            if (y + ctx.fontSize > ctx.textDisplayArea.y + ctx.textDisplayArea.height) break;
            DrawTextEx(GetFontDefault(), ln.c_str(), {ctx.textDisplayArea.x, y}, (float)ctx.fontSize, 1.0f, BLACK);
            y += lineHeight;
        }

        if (ctx.currentBook) {
            int pageInfoWidth = MeasureText(ctx.pageInfoText.c_str(), ctx.fontSize);
            DrawText(ctx.pageInfoText.c_str(), (ctx.screen_width - pageInfoWidth) / 2, ctx.screen_height - ctx.navButtonMargin - ctx.navButtonHeight / 2, ctx.fontSize, DARKGRAY);

            bool canGoPrev = (ctx.currentPageNum > 1);
            DrawRectangleRec(ctx.prevPageButtonRect, canGoPrev ? LIGHTGRAY : DARKGRAY);
            DrawText("<", ctx.prevPageButtonRect.x + ctx.navButtonWidth / 2 - MeasureText("<", 30) / 2, ctx.prevPageButtonRect.y + ctx.navButtonHeight / 2 - 15, 30, canGoPrev ? BLACK : GRAY);

            bool canGoNext = (ctx.currentPageNum < ctx.totalPages);
            DrawRectangleRec(ctx.nextPageButtonRect, canGoNext ? LIGHTGRAY : DARKGRAY);
            DrawText(">", ctx.nextPageButtonRect.x + ctx.navButtonWidth / 2 - MeasureText(">", 30) / 2, ctx.nextPageButtonRect.y + ctx.navButtonHeight / 2 - 15, 30, canGoNext ? BLACK : GRAY);
        }
    }

    DrawRectangle(ctx.panel_x, 0, ctx.panel_width, ctx.screen_height, BLACK);

    // panel Toggle Button
    int toggle_button_x = (ctx.panel_x <= -ctx.panel_width) ? ctx.panel_toggle_button_offset : ctx.panel_x + ctx.panel_width - ctx.panel_toggle_button_width - ctx.panel_toggle_button_offset;
    DrawRectangle(toggle_button_x, ctx.panel_toggle_button_offset, ctx.panel_toggle_button_width, ctx.panel_toggle_button_height, ctx.panel_toggle_color);
    DrawText(ctx.panel_toggle_icon.c_str(), toggle_button_x + 10, ctx.panel_toggle_button_offset + 5, 20, ctx.panel_toggle_text_color);

    // panel Content Buttons (only if visible)
    if (ctx.panel_x > -ctx.panel_width + 5) {
        int open_button_x = ctx.panel_x + ctx.panel_content_button_offset;
        DrawRectangle(open_button_x, ctx.open_button_y_offset, ctx.panel_button_width, ctx.panel_button_height, DARKGRAY);
        DrawText("Open Book (.txt)", open_button_x + (ctx.panel_button_width - MeasureText("Open Book (.txt)", 20)) / 2, ctx.open_button_y_offset + 10, 20, WHITE);

        int exit_button_x = ctx.panel_x + ctx.panel_content_button_offset;
        int exit_button_y = ctx.screen_height - ctx.panel_button_height - ctx.panel_content_button_offset;
        DrawRectangle(exit_button_x, exit_button_y, ctx.panel_button_width, ctx.panel_button_height, RED);
        DrawText("EXIT", exit_button_x + (ctx.panel_button_width - MeasureText("EXIT", 20)) / 2, exit_button_y + 10, 20, WHITE);
    }

    EndDrawing();
}
