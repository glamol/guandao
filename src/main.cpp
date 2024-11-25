#include "raylib.h"
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
  bool panel_visible = false;
  bool is_animating = false;

  const int exit_button_width = panel_width - 2 * button_offset;
  const int exit_button_height = 40;
  const int exit_button_y = screen_height - exit_button_height - button_offset;

  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    if (panel_x + panel_width - button_width - button_offset >= button_offset)
      button_x = panel_x + panel_width - button_width - button_offset;
    else
      button_x = button_offset;

    int exit_button_x = panel_x + button_offset;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      Vector2 mouse_position = GetMousePosition();
      Rectangle button_rect = {(float)button_x, (float)button_y,
                               (float)button_width, (float)button_height};
      if (CheckCollisionPointRec(mouse_position, button_rect) &&
          !is_animating) {
        panel_visible = !panel_visible;
        is_animating = true;
        button_icon = panel_visible ? "<" : ">";
      }
      Rectangle exit_button_rect = {(float)exit_button_x, (float)exit_button_y,
                                    (float)exit_button_width,
                                    (float)exit_button_height};
      if (CheckCollisionPointRec(mouse_position, exit_button_rect)) {
        CloseWindow();
        return 0;
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

    DrawRectangle(panel_x, 0, panel_width, screen_height, BLACK);
    DrawRectangle(button_x, button_y, button_width, button_height, BLACK);
    DrawText(button_icon.c_str(), button_x + 10, button_y + 5, 20, WHITE);

    if (panel_x > -panel_width + panel_animation_speed) {
      DrawRectangle(exit_button_x, exit_button_y, exit_button_width,
                    exit_button_height, RED);
      DrawText("EXIT",
               exit_button_x +
                   (exit_button_width - MeasureText("EXIT", 20)) / 2,
               exit_button_y + 10, 20, WHITE);
    }

    DrawText("GUANDAO", (screen_width - MeasureText("GUANDAO", 40)) / 2,
             screen_height / 2 - 20, 40, BLACK);
    EndDrawing();
  }

  CloseWindow();
  std::cout << screen_height << "  " << screen_width;

  return 0;
}
