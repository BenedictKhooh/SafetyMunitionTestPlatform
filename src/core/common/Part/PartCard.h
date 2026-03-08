#pragma once
#include "src/core/common/KeywordCard.h"
#include <string>

class PartCard : public KeywordCard {
private:
    std::string title;
    int pid, secid, mid, eosid, hgid;
    int elform;
public:
    PartCard(std::string t, int p, int s, int m, int e, int h, int el = 1);
    std::string generate() const override;
};
