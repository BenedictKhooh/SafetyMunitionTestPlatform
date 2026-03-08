#include "DatabaseCards.h"
#include "src/core/common/DynaFormat.h"
#include <sstream>

std::string D3PlotCard::generate() const {
    std::stringstream ss;
    ss << "*DATABASE_BINARY_D3PLOT\n$#      dt      lcdt      beam     npltc    psetid\n";
    ss << DynaFormat::F10(dt) << "         0         0         0         0\n";
    return ss.str();
}

std::string ExtentBinaryCard::generate() const {
    std::stringstream ss;
    ss << "*DATABASE_EXTENT_BINARY\n";
    ss << "$#   neiph     neips    maxint    strflg    sigflg    epsflg    rltflg    engflg\n";
    ss << "        50         8         0         0         1         1         1         1\n";
    ss << "$#  cmpflg    ieverp    beamip     dcomp      shge     stssz    n3thdt   ialemat\n";
    ss << "         0         1         0         1         1         1         2         0\n";
    return ss.str();
}