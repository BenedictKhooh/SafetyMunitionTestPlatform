#pragma once
#include "src/core/common/DynaFormat.h"
#include "src/core/common/KeywordCard.h"
#include <sstream>

class HourglassCard : public KeywordCard {
public:
    int hgid;
    HourglassCard(int id) : hgid(id) {}
    std::string generate() const override {
        std::stringstream ss;
        ss << "*HOURGLASS\n$#    hgid       ihq        qm       ibq        q1        q2    qb/vdc        qw\n";
        ss << DynaFormat::I10(hgid) << "         2       0.1         0       1.5      0.06       0.1       0.1\n";
        return ss.str();
    }
};