#include "utils.h"
#include "book_manager.h"
#include <sstream>
#include <iostream>
#include <vector>
#include <cstddef>

void DrawWrappedText(Font font, const std::string& text, Rectangle rec, float fontSize, float lineSpacingFactor, Color color) {
    float currentY = rec.y;
    std::string remainingText = text;
    std::string lineToDraw;
    float lineHeight = fontSize * lineSpacingFactor;
    while (!remainingText.empty() && currentY < rec.y + rec.height - fontSize) {
        lineToDraw = ""; std::string wordBuffer; std::stringstream ss(remainingText);
        float currentLineWidth = 0; bool firstWord = true;
        std::string nextLoopRemainingText = ""; bool lineCompletelyProcessed = true;
        while (ss >> wordBuffer) {
            size_t newlinePos = wordBuffer.find('\n'); std::string wordPart = wordBuffer; bool hasExplicitNewline = false;
            if (newlinePos != std::string::npos) {
                wordPart = wordBuffer.substr(0, newlinePos); hasExplicitNewline = true;
                std::string afterNewline = wordBuffer.substr(newlinePos + 1); std::string restOfStream; std::getline(ss, restOfStream);
                nextLoopRemainingText = afterNewline + restOfStream; lineCompletelyProcessed = false;
            }
            std::string wordWithSpace = (firstWord ? "" : " ") + wordPart; Vector2 wordSize = MeasureTextEx(font, wordWithSpace.c_str(), fontSize, 1.0f);
            if (lineToDraw.empty() && wordSize.x > rec.width) {
                int charsFit = 0;
                for (size_t i = 1; i <= wordPart.length(); ++i) {
                    if (MeasureTextEx(font, wordPart.substr(0, i).c_str(), fontSize, 1.0f).x <= rec.width) {
                        charsFit = (int)i;
                    } else {
                        break;
                    }
                }
                if (charsFit > 0) {
                     lineToDraw = wordPart.substr(0, charsFit);
                 } else {
                     lineToDraw = "";
                 }
                std::string restOfWord = wordPart.substr(charsFit);
                std::string restOfStream;
                std::getline(ss, restOfStream);
                nextLoopRemainingText = restOfWord + (hasExplicitNewline ? "\n" : "") + restOfStream;
                lineCompletelyProcessed = false;
                break;
            } else if (currentLineWidth + wordSize.x <= rec.width) {
                lineToDraw += wordWithSpace; currentLineWidth += wordSize.x; firstWord = false;
                if (hasExplicitNewline) break;
            } else {
                std::string restOfStream; std::getline(ss, restOfStream);
                nextLoopRemainingText = wordBuffer + restOfStream;
                lineCompletelyProcessed = false;
                break;
            }
        }
        if (lineCompletelyProcessed) { nextLoopRemainingText = ""; }
        DrawTextEx(font, lineToDraw.c_str(), {rec.x, currentY}, fontSize, 1.0f, color);
        currentY += lineHeight; remainingText = nextLoopRemainingText;
        size_t firstChar = remainingText.find_first_not_of(" \t\n\r");
        if (firstChar != std::string::npos) { remainingText = remainingText.substr(firstChar); } else { remainingText = ""; }
    }
}

Book* LoadBookFromFile(const std::string& filePath) {
    std::cout << "Attempting to load book: " << filePath << std::endl;
    Book* newBook = new Book(filePath);

    if (newBook != nullptr && newBook->isValid()) {
        std::cout << "Book loaded successfully (" << newBook->getLength() << " pages)." << std::endl;
        return newBook;
    } else {
        std::cerr << "Failed to load book or book is empty/invalid: " << filePath << std::endl;
        delete newBook;
        return nullptr;
    }
}
