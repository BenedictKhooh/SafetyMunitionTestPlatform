#include "BoundaryCards.h"
#include "src/core/common/DynaFormat.h"
#include <sstream>

void SpcSetCard::addConstraint(int nsid, int x, int y, int z, int rx, int ry, int rz) {
    constraints.push_back({ nsid, x, y, z, rx, ry, rz });
}

std::string SpcSetCard::generate() const {
    std::stringstream ss;
    ss << "*BOUNDARY_SPC_SET\n";
    for (const auto& c : constraints) {
        ss << "$#    nsid       cid      dofx      dofy      dofz     dofrx     dofry     dofrz\n";
        ss << DynaFormat::I10(c.nsid) << DynaFormat::I10(0)
            << DynaFormat::I10(c.dofx) << DynaFormat::I10(c.dofy) << DynaFormat::I10(c.dofz)
            << DynaFormat::I10(c.dofrx) << DynaFormat::I10(c.dofry) << DynaFormat::I10(c.dofrz) << "\n";
    }
    return ss.str();
}