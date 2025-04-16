#ifndef BOOK_MANAGER_H
#define BOOK_MANAGER_H

#include <string>
#include <vector>
#include <cstddef>
class Book {
public:
    Book(const std::string& filePath);
    int getLength() const;
    std::string getCurrentPage() const;
    int getCurrentPageNumber() const;
    void nextPage();
    void previousPage();
    bool isValid() const;

private:
    std::vector<std::string> pages;
    size_t currentPageIndex;
};

#endif // BOOK_MANAGER_H
