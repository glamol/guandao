#ifndef UTILS_H
#define UTILS_H

#include "raylib.h"
#include <string>

class Book;

void DrawWrappedText(Font font,
                     const std::string& text,
                     Rectangle rec,
                     float fontSize,
                     float lineSpacingFactor,
                     Color color);

Book* LoadBookFromFile(const std::string& filePath);

#endif // UTILS_H
