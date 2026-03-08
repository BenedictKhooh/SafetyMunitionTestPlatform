#pragma once
#include "src/core/common/KeywordCard.h"
#include <vector>

struct SPC {
    int nsid;
    int dofx, dofy, dofz, dofrx, dofry, dofrz;
};

class SpcSetCard : public KeywordCard {
private:
    std::vector<SPC> constraints;
public:
    void addConstraint(int nsid, int x, int y, int z, int rx, int ry, int rz);
    std::string generate() const override;
};