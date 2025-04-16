#include "raylib.h"
#include "tinyfiledialogs.h"
#include "book_manager.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <vector>

int main(void) {
  int monitor_num = 0;
  int screen_width = 800;
  int screen_height = 600;

  InitWindow(screen_width, screen_height, "GUANDAO - Book Reader");

  const int panel_width = 200;
  const int panel_toggle_button_width = 30;
  const int panel_toggle_button_height = 30;
  const int panel_toggle_button_offset = 5;
  const int panel_animation_speed = 10;
  int panel_x = -panel_width;
  int panel_toggle_button_x = panel_toggle_button_offset;
  int panel_toggle_button_y = panel_toggle_button_offset;
  std::string panel_toggle_icon = ">";
  CLITERAL(Color) panel_toggle_color = BLACK;
  CLITERAL(Color) panel_toggle_text_color = RAYWHITE;
  bool panel_visible = false;
  bool is_animating = false;

  const int panel_content_button_offset = 5;
  const int panel_button_width = panel_width - 2 * panel_content_button_offset;
  const int panel_button_height = 40;
  const int open_button_y_offset = panel_toggle_button_offset + panel_toggle_button_height + 10;

  Book* currentBook = nullptr;
  std::string currentPageText = "";
  std::string pageInfoText = "";
  int currentPageNum = 0;
  int totalPages = 0;
  bool bookLoadAttemptedOrSuccess = false;

  const float textMarginHorizontal = 30.0f;
  const float textMarginTop = 30.0f;
  const float textMarginBottom = 80.0f;
  const int fontSize = 20;
  const float textLineSpacingFactor = 1.2f;
  Rectangle textDisplayArea = { textMarginHorizontal, textMarginTop, screen_width - 2 * textMarginHorizontal, screen_height - textMarginTop - textMarginBottom };

  const float navButtonWidth = 50.0f;
  const float navButtonHeight = 50.0f;
  const float navButtonMargin = 20.0f;
  Rectangle prevPageButtonRect = { navButtonMargin, screen_height - navButtonHeight - navButtonMargin, navButtonWidth, navButtonHeight };
  Rectangle nextPageButtonRect = { screen_width - navButtonWidth - navButtonMargin, screen_height - navButtonHeight - navButtonMargin, navButtonWidth, navButtonHeight };

  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    Vector2 mouse_position = GetMousePosition();
    bool panelWasClicked = false;

    if (panel_x + panel_width - panel_toggle_button_width - panel_toggle_button_offset >= panel_toggle_button_offset)
      panel_toggle_button_x = panel_x + panel_width - panel_toggle_button_width - panel_toggle_button_offset;
    else panel_toggle_button_x = panel_toggle_button_offset;
    panel_toggle_button_y = panel_toggle_button_offset;
    int exit_button_x = panel_x + panel_content_button_offset;
    int exit_button_y = screen_height - panel_button_height - panel_content_button_offset;
    int open_button_x = panel_x + panel_content_button_offset;
    int open_button_y = open_button_y_offset;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Rectangle panel_toggle_rect = {(float)panel_toggle_button_x, (float)panel_toggle_button_y, (float)panel_toggle_button_width, (float)panel_toggle_button_height};
        if (CheckCollisionPointRec(mouse_position, panel_toggle_rect) && !is_animating) {
             panel_visible = !panel_visible; is_animating = true; panel_toggle_icon = panel_visible ? "<" : ">";
             panel_toggle_color = panel_visible ? RAYWHITE : BLACK; panel_toggle_text_color = panel_visible ? BLACK : RAYWHITE;
             panelWasClicked = true;
        }

        if (panel_x > -panel_width + 5) {
            Rectangle exit_button_rect = {(float)exit_button_x, (float)exit_button_y, (float)panel_button_width, (float)panel_button_height};
            if (CheckCollisionPointRec(mouse_position, exit_button_rect) && !WindowShouldClose()) {
                panelWasClicked = true;
                delete currentBook; currentBook = nullptr;
                CloseWindow();
                return 0;
            }

            Rectangle open_button_rect = {(float)open_button_x, (float)open_button_y, (float)panel_button_width, (float)panel_button_height};
            if (CheckCollisionPointRec(mouse_position, open_button_rect)) {
                const char *filters[] = {"*.txt"};
                const char *file = tinyfd_openFileDialog("Select Book (.txt)", "", 1, filters, "Text Files", 0);

                bookLoadAttemptedOrSuccess = true;
                panelWasClicked = true;

                if (file) {
                    Book* loadedBook = LoadBookFromFile(file);
                    if (loadedBook != nullptr) {
                        delete currentBook;
                        currentBook = loadedBook;
                        currentPageText = currentBook->getCurrentPage();
                        currentPageNum = currentBook->getCurrentPageNumber();
                        totalPages = currentBook->getLength();
                        pageInfoText = "Page " + std::to_string(currentPageNum) + " / " + std::to_string(totalPages);
                         if (panel_visible && !is_animating) {
                            panel_visible = false; is_animating = true;
                            panel_toggle_icon = ">"; panel_toggle_color = BLACK; panel_toggle_text_color = RAYWHITE;
                        }
                    } else {
                        currentPageText = "Error loading book:\n";
                        currentPageText += file;
                        currentPageText += "\n\nPlease ensure the file exists, is readable,\nand contains '<PAGE_BREAK>' markers.";
                        pageInfoText = "Book Error";
                        currentPageNum = 0;
                        totalPages = 0;
                    }
                } else {
                     std::cout << "File selection cancelled." << std::endl;
                }
            }
        }

        if (currentBook != nullptr && !panelWasClicked) {
            bool navigated = false;
            if (CheckCollisionPointRec(mouse_position, nextPageButtonRect) && currentPageNum < totalPages) {
                currentBook->nextPage(); navigated = true;
            } else if (CheckCollisionPointRec(mouse_position, prevPageButtonRect) && currentPageNum > 1) {
                currentBook->previousPage(); navigated = true;
            }
            if (navigated) {
                currentPageText = currentBook->getCurrentPage();
                currentPageNum = currentBook->getCurrentPageNumber();
                pageInfoText = "Page " + std::to_string(currentPageNum) + " / " + std::to_string(totalPages);
            }
        }
    }

    if (currentBook != nullptr) {
         bool navigated = false;
         if (IsKeyPressed(KEY_RIGHT) && currentPageNum < totalPages) {
             currentBook->nextPage(); navigated = true;
         }
         if (IsKeyPressed(KEY_LEFT) && currentPageNum > 1) {
             currentBook->previousPage(); navigated = true;
         }
         if (navigated) {
             currentPageText = currentBook->getCurrentPage();
             currentPageNum = currentBook->getCurrentPageNumber();
             pageInfoText = "Page " + std::to_string(currentPageNum) + " / " + std::to_string(totalPages);
         }
    }

    if (is_animating) {
        if (panel_visible) {
            panel_x += panel_animation_speed;
            if (panel_x >= 0) { panel_x = 0; is_animating = false; }
        } else {
            panel_x -= panel_animation_speed;
            if (panel_x <= -panel_width) { panel_x = -panel_width; is_animating = false; }
        }
    }
    float currentPanelOffset = (panel_x > -panel_width) ? (panel_width + panel_x) : 0;
    textDisplayArea.x = textMarginHorizontal + currentPanelOffset;
    textDisplayArea.width = screen_width - textDisplayArea.x - textMarginHorizontal;
    if (textDisplayArea.width < 10) textDisplayArea.width = 10;


    BeginDrawing();
      ClearBackground(RAYWHITE);

      if (!bookLoadAttemptedOrSuccess) {
            const char* previewText = "GUANDAO";
            int previewFontSize = 40;
            int textWidth = MeasureText(previewText, previewFontSize);
            DrawText(previewText, (screen_width - textWidth) / 2, screen_height / 2 - previewFontSize / 2, previewFontSize, LIGHTGRAY);
            const char* instructionText = "Use Panel '>' to Open Book (.txt)";
            int instructionFontSize = 18;
            int instructionWidth = MeasureText(instructionText, instructionFontSize);
            DrawText(instructionText, (screen_width-instructionWidth)/2, screen_height/2 + previewFontSize/2 + 10, instructionFontSize, GRAY);
      } else {
            DrawWrappedText(GetFontDefault(), currentPageText, textDisplayArea, (float)fontSize, textLineSpacingFactor, BLACK);
            if (currentBook != nullptr) {
                int pageInfoWidth = MeasureText(pageInfoText.c_str(), fontSize);
                DrawText(pageInfoText.c_str(), (screen_width - pageInfoWidth) / 2, screen_height - navButtonMargin - navButtonHeight / 2 , fontSize, DARKGRAY);
                bool canGoPrev = (currentPageNum > 1);
                DrawRectangleRec(prevPageButtonRect, canGoPrev ? LIGHTGRAY : DARKGRAY);
                DrawText("<", prevPageButtonRect.x + navButtonWidth/2 - MeasureText("<", 30)/2, prevPageButtonRect.y + navButtonHeight/2 - 15, 30, canGoPrev ? BLACK : GRAY);
                bool canGoNext = (currentPageNum < totalPages);
                DrawRectangleRec(nextPageButtonRect, canGoNext ? LIGHTGRAY : DARKGRAY);
                DrawText(">", nextPageButtonRect.x + navButtonWidth/2 - MeasureText(">", 30)/2, nextPageButtonRect.y + navButtonHeight/2 - 15, 30, canGoNext ? BLACK : GRAY);
            }
      }

      DrawRectangle(panel_x, 0, panel_width, screen_height, BLACK);
      DrawRectangle(panel_toggle_button_x, panel_toggle_button_y, panel_toggle_button_width, panel_toggle_button_height, panel_toggle_color);
      DrawText(panel_toggle_icon.c_str(), panel_toggle_button_x + 10, panel_toggle_button_y + 5, 20, panel_toggle_text_color);
      if (panel_x > -panel_width + 5) {
          DrawRectangle(open_button_x, open_button_y, panel_button_width, panel_button_height, DARKGRAY);
          DrawText("Open Book (.txt)", open_button_x + (panel_button_width - MeasureText("Open Book (.txt)", 20)) / 2, open_button_y + 10, 20, WHITE);
          DrawRectangle(exit_button_x, exit_button_y, panel_button_width, panel_button_height, RED);
          DrawText("EXIT", exit_button_x + (panel_button_width - MeasureText("EXIT", 20)) / 2, exit_button_y + 10, 20, WHITE);
      }

    EndDrawing();
  }

  delete currentBook;

  return 0;
}
