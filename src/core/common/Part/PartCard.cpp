#include "PartCard.h"
#include "src/core/common/DynaFormat.h"
#include <sstream>

PartCard::PartCard(std::string t, int p, int s, int m, int e, int h, int el)
    : title(t), pid(p), secid(s), mid(m), eosid(e), hgid(h), elform(el) {
}

std::string PartCard::generate() const {
    std::stringstream ss;
    ss << "*PART\n";
    ss << "$#                                                                         title\n";
    ss << title << "\n";
    ss << "$#      pid     secid       mid     eosid      hgid      grav    adpopt      tmid\n";
    ss << DynaFormat::I10(pid) << DynaFormat::I10(secid) << DynaFormat::I10(mid)
        << DynaFormat::I10(eosid) << DynaFormat::I10(hgid) << DynaFormat::I10(0)
        << DynaFormat::I10(0) << DynaFormat::I10(0) << "\n";

    ss << "*SECTION_SOLID_TITLE\n";
    ss << title << "\n";
    ss << "$#   secid    elform       aet\n";
    ss << DynaFormat::I10(secid) << DynaFormat::I10(elform) << DynaFormat::I10(0) << "\n";
    return ss.str();
}