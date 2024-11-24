#include "raylib.h"

int main(void) {
  int screen_width = 800;
  int screen_height = 600;

  // texture for background
  InitWindow(screen_width, screen_height, "GUANDAO");
  Image bg_image = LoadImage("resources/background.png");
  Texture2D bg_texture = LoadTextureFromImage(bg_image);
  UnloadImage(bg_image);

  // resizing
  Rectangle bg_source = {0, 0, static_cast<float>(bg_texture.width),
                         static_cast<float>(bg_texture.height)};
  Rectangle bg_dest = {0, 0, static_cast<float>(screen_width),
                       static_cast<float>(screen_height)};
  Vector2 bg_origin = {0, 0};

  SetTargetFPS(60);
  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(BLACK);

    DrawTexturePro(bg_texture, bg_source, bg_dest, bg_origin, 0.0f, WHITE);

    EndDrawing();
  }

  UnloadTexture(bg_texture); // cleaning
  CloseWindow();

  return 0;
}
