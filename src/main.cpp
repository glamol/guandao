#include "raylib.h"

int main(void) {
  int Monitor_num = GetMonitorCount();
  InitWindow(GetMonitorWidth(Monitor_num), GetMonitorHeight(Monitor_num), "raylib [core] example - basic window");
  
  
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Congrats! You created your first window!", 190, 200, 20,
             LIGHTGRAY);
    EndDrawing();
  }

  CloseWindow();

  return 0;
}
