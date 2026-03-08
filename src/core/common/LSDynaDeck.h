#pragma once
#include "KeywordCard.h"
#include <vector>
#include <string>
#include <memory>

class LSDynaDeck {
private:
    std::vector<std::shared_ptr<KeywordCard>> deck;
    std::string meshIncludePath;
public:
    void addCard(std::shared_ptr<KeywordCard> card);
    void setMeshInclude(const std::string& path);
    void writeToFile(const std::string& filename);
};
