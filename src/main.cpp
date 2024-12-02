#include "raylib.h"
#include "tinyfiledialogs.h"
#include <iostream>
#include <string>

class Button{
  private:
  //x, y, width, height
    
    Color button_color = RAYWHITE;
    //text
    std::string text = "";
    int text_font_size = 20;
    Color text_color = BLACK;

  public:
    Rectangle rect;
    Button(float width, float height){
      rect.width = width;
      rect.height = height;
    }
    Button(float x, float y, float width, float height){
      rect.x = x;
      rect.y = y;
      rect.width = width;
      rect.height = height;
    }
    Button(float x, float y, float width, float height, Color color){
      rect.x = x;
      rect.y = y;
      rect.width = width;
      rect.height = height;
      button_color = color;
    }
    Button(float x, float y, float width, float height, Color color, std::string txt, int font, Color txt_color){
      rect.x = x;
      rect.y = y;
      rect.width = width;
      rect.height = height;
      button_color = color;

      text = txt;
      text_font_size = font;
      text_color = txt_color; 
    }
    //setters
    void setx(float x){
      rect.x = x;
    }
    void sety(float y){
      rect.y = y;
    }

    void set_color(Color color){
      button_color = color;
    }
    void set_text_settings(std::string txt, Color color){
      text = txt;
      text_color = color;
    }
    void set_text_settings(std::string txt, Color color, int font){
      text = txt;
      text_color = color;
      text_font_size = font;
    }

    void draw(int mode){ // 0 - without text, 1 - centered text, 2 - ?? (may be added)
      DrawRectangleRec(rect, button_color);
      //or change to switch/case construction
      if (mode == 1) DrawText(text.c_str(), 
                              rect.x + (rect.width - MeasureText(text.c_str(), 20)) / 2, 
                              rect.y + rect.height / 4, 
                              text_font_size, text_color);
    }
    bool is_clicked(Vector2 mouse_position){
      return CheckCollisionPointRec(mouse_position, rect);
    }
};

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

  Button exit_button((float)panel_width - 2 * button_offset, (float)40); 

  const int top_button_x = button_offset;
  Button top_button(button_offset, button_offset + button_height + 10,
                          panel_width - 2 * button_offset, 40,
                          DARKGRAY, 
                          "Open File", 20, WHITE);
  Button rect_button(button_x, button_y, button_width, button_height, button_color);
  rect_button.set_text_settings(">", RAYWHITE);
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    if (panel_x + panel_width - button_width - button_offset >= button_offset)
      button_x = panel_x + panel_width - button_width - button_offset;
    else
      button_x = button_offset;
    
    exit_button.rect.x = panel_x + button_offset;
    exit_button.rect.y = screen_height - exit_button.rect.height - button_offset;
    //exit_button.setx(panel_x + button_offset);
    //exit_button.sety(screen_height - exit_button.height - button_offset);

    top_button.rect.x = panel_x + top_button_x;
    rect_button.rect.x = button_x;
     
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      Vector2 mouse_position = GetMousePosition();
      //Rectangle button_rect = {(float)button_x, (float)button_y, (float)button_width, (float)button_height};
      if (rect_button.is_clicked(mouse_position) && !is_animating) {
        panel_visible = !panel_visible;
        is_animating = true;
        panel_visible ? rect_button.set_text_settings("<", BLACK) : rect_button.set_text_settings(">", RAYWHITE);
        panel_visible ? rect_button.set_color(RAYWHITE) : rect_button.set_color(BLACK);
      }

      if (exit_button.is_clicked(mouse_position)) {
         CloseWindow(); 
         return 0;
      }
      
      if (top_button.is_clicked(mouse_position)) {
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

    // panel
    DrawRectangle(panel_x, 0, panel_width, screen_height, BLACK);

    rect_button.draw(1);
    if (panel_x > -panel_width + panel_animation_speed) {
      // file button
      top_button.draw(1);
      
      // exit button
      exit_button.set_color(RED);
      exit_button.set_text_settings("EXIT", WHITE);
      exit_button.draw(1);
    }

    DrawText("GUANDAO", (screen_width - MeasureText("GUANDAO", 48)) / 2,
             screen_height / 2 - 24, 40, BLACK);
    EndDrawing();
  }

  CloseWindow();
  std::cout << screen_height << "  " << screen_width;

  return 0;
}
