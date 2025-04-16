#include "book_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

Book::Book(const std::string& filePath) : currentPageIndex(0) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return;
    }

    std::string line;
    std::stringstream current_page_content;

    while (std::getline(file, line)) {
        if (line == "<PAGE_BREAK>") {
            this->pages.push_back(current_page_content.str());
            current_page_content.str("");
            current_page_content.clear();
        } else {
            current_page_content << line << "\n";
        }
    }

    if (!current_page_content.str().empty()) {
        this->pages.push_back(current_page_content.str());
    }

    file.close();

    if (this->pages.empty()) {
        std::cerr << "Warning: Book '" << filePath << "' is empty or contains no page breaks." << std::endl;
        currentPageIndex = 0;
    }
}

int Book::getLength() const {
    return this->pages.size();
}

std::string Book::getCurrentPage() const {
    if (!this->pages.empty() && this->currentPageIndex < this->pages.size()) {
        return this->pages[this->currentPageIndex];
    }
    if (this->pages.empty()) {
         return "Error: Book not loaded or empty.";
    }
    return "Error: Invalid page index.";
}

int Book::getCurrentPageNumber() const {
    if (this->pages.empty()) {
        return 0;
    }
    return this->currentPageIndex + 1;
}

void Book::nextPage() {
    if (!this->pages.empty() && this->currentPageIndex < this->pages.size() - 1) {
        this->currentPageIndex++;
    }
}

void Book::previousPage() {
    if (this->currentPageIndex > 0) {
        this->currentPageIndex--;
    }
}

bool Book::isValid() const {
    return !this->pages.empty();
}
