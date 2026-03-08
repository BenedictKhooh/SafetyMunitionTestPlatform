#include "ContactCard.h"
#include "src/core/common/DynaFormat.h"
#include <sstream>

ContactESTSCard::ContactESTSCard(int s, int m, double stat_fric, double dyn_fric)
    : ssid(s), msid(m), fs(stat_fric), fd(dyn_fric) {
}

std::string ContactESTSCard::generate() const {
    std::stringstream ss;
    ss << "*CONTACT_ERODING_SURFACE_TO_SURFACE\n";
    ss << "$#    ssid      msid     sstyp     mstyp    sboxid    mboxid       spr       mpr\n";
    ss << DynaFormat::I10(ssid) << DynaFormat::I10(msid) << DynaFormat::I10(3) << DynaFormat::I10(3)
        << DynaFormat::I10(0) << DynaFormat::I10(0) << DynaFormat::I10(1) << DynaFormat::I10(1) << "\n";

    ss << "$#      fs        fd        dc        vc       vdc    penchk        bt        dt\n";
    ss << DynaFormat::F10(fs) << DynaFormat::F10(fd) << DynaFormat::F10(0.0) << DynaFormat::F10(0.0017)
        << DynaFormat::F10(20.0) << DynaFormat::I10(2) << DynaFormat::F10(0.0) << DynaFormat::E10(1e20) << "\n";
    return ss.str();
}