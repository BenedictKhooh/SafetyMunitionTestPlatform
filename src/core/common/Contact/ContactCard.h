#pragma once
#include "src/core/common/KeywordCard.h"

class ContactESTSCard : public KeywordCard {
private:
    int ssid, msid;
    double fs, fd;
public:
    ContactESTSCard(int s, int m, double stat_fric, double dyn_fric);
    std::string generate() const override;
};
