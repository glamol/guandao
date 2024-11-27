#include "raylib.h"
#include "tinyfiledialogs.h"
#include <iostream>
#include <string>

int main(void) {
  int monitor_num = 0;
  //  WARNING: GLFW: Failed to find selected monitor
  //
  /*std::cout<<GetMonitorCount();*/
  int screen_width = 800;  // GetMonitorWidth(monitor_num);
  int screen_height = 600; // GetMonitorHeight(monitor_num);

  InitWindow(screen_width, screen_height, "GUANDAO");

  const int panel_width = 200;
  const int button_width = 30;
  const int button_height = 30;
  const int button_offset = 5;
  const int panel_animation_speed = 10;
  int panel_x = -panel_width;
  int button_x = button_offset;
  int button_y = button_offset;
  std::string button_icon = ">";
  CLITERAL(Color) button_color = BLACK;
  CLITERAL(Color) button_text_color = RAYWHITE;
  bool panel_visible = false;
  bool is_animating = false;

  const int exit_button_width = panel_width - 2 * button_offset;
  const int exit_button_height = 40;
  const int exit_button_y = screen_height - exit_button_height - button_offset;

  const int top_button_width = panel_width - 2 * button_offset;
  const int top_button_height = 40;
  const int top_button_x = button_offset;
  const int top_button_y = button_offset + button_height + 10;
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    if (panel_x + panel_width - button_width - button_offset >= button_offset)
      button_x = panel_x + panel_width - button_width - button_offset;
    else
      button_x = button_offset;

    int exit_button_x = panel_x + button_offset;
    int actual_top_button_x = panel_x + top_button_x;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      Vector2 mouse_position = GetMousePosition();
      Rectangle button_rect = {(float)button_x, (float)button_y,
                               (float)button_width, (float)button_height};
      if (CheckCollisionPointRec(mouse_position, button_rect) &&
          !is_animating) {
        panel_visible = !panel_visible;
        is_animating = true;
        button_icon = panel_visible ? "<" : ">";
        button_color = panel_visible ? RAYWHITE : BLACK;
        button_text_color = panel_visible ? BLACK : RAYWHITE;

      }
      Rectangle exit_button_rect = {(float)exit_button_x, (float)exit_button_y,
                                    (float)exit_button_width,
                                    (float)exit_button_height};
      if (CheckCollisionPointRec(mouse_position, exit_button_rect)) {
        CloseWindow();
        return 0;
      }
      Rectangle top_button_rect = {(float)actual_top_button_x,
                                   (float)top_button_y, (float)top_button_width,
                                   (float)top_button_height};
      if (CheckCollisionPointRec(mouse_position, top_button_rect)) {
        const char *filters[] = {"*.*"};
        const char *file =
            tinyfd_openFileDialog("Select a file", "", 0, filters, NULL, 0);
        if (file) {
          // TODO: implement file procces
          std::cout << "Selected file: " << file << std::endl;
        }
      }
    }

    if (is_animating) {
      if (panel_visible) {
        panel_x += panel_animation_speed;
        if (panel_x >= 0) {
          panel_x = 0;
          is_animating = false;
        }
      } else {
        panel_x -= panel_animation_speed;
        if (panel_x <= -panel_width) {
          panel_x = -panel_width;
          is_animating = false;
        }
      }
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // panel and toggle button
    DrawRectangle(panel_x, 0, panel_width, screen_height, BLACK);
    DrawRectangle(button_x, button_y, button_width, button_height, button_color);
    DrawText(button_icon.c_str(), button_x + 10, button_y + 5, 20, button_text_color);

    if (panel_x > -panel_width + panel_animation_speed) {
      // file button
      DrawRectangle(actual_top_button_x, top_button_y, top_button_width,
                    top_button_height, DARKGRAY);
      DrawText("Open File",
               actual_top_button_x +
                   (top_button_width - MeasureText("Open File", 20)) / 2,
               top_button_y + 10, 20, WHITE);
      // exit button
      DrawRectangle(exit_button_x, exit_button_y, exit_button_width,
                    exit_button_height, RED);
      DrawText("EXIT",
               exit_button_x +
                   (exit_button_width - MeasureText("EXIT", 20)) / 2,
               exit_button_y + 10, 20, WHITE);
    }

    DrawText("GUANDAO", (screen_width - MeasureText("GUANDAO", 48)) / 2,
             screen_height / 2 - 24, 40, BLACK);
    EndDrawing();
  }

  CloseWindow();
  std::cout << screen_height << "  " << screen_width;

  return 0;
}
