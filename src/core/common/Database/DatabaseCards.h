#pragma once
#include "src/core/common/KeywordCard.h"

class D3PlotCard : public KeywordCard {
private:
    double dt;
public:
    D3PlotCard(double interval) : dt(interval) {}
    std::string generate() const override;
};

class ExtentBinaryCard : public KeywordCard {
public:
    std::string generate() const override;
};