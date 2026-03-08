#include "ControlCards.h"
#include "src/core/common/DynaFormat.h"
#include <sstream>

std::string BulkViscosityCard::generate() const {
    std::stringstream ss;
    ss << "*CONTROL_BULK_VISCOSITY\n$#      q1        q2      type     btype    tstype\n";
    ss << DynaFormat::F10(1.5) << DynaFormat::F10(0.06) << DynaFormat::I10(1) << DynaFormat::I10(0) << DynaFormat::I10(0) << "\n";
    return ss.str();
}

std::string ControlContactCard::generate() const {
    std::stringstream ss;
    ss << "*CONTROL_CONTACT\n";
    ss << "$#  slsfac    rwpnal    islchk    shlthk    penopt    thkchg     orien    enmass\n";
    ss << "       0.2       0.0         2         0         1         1         1         0\n";
    ss << "$#  usrstr    usrfrc     nsbcs    interm     xpene     ssthk      ecdt   tiedprj\n";
    ss << "         0         0         0         0       4.0         0         0         0\n";
    ss << "$#   sfric     dfric       edc       vfc        th     th_sf    pen_sf\n";
    ss << "       0.0       0.0       0.0       0.0       0.0       0.0       0.0\n";
    ss << "$#  ignore    frceng   skiprwg    outseg   spotstp   spotdel   spothin\n";
    ss << "         0         1         0         0         2         0       0.0\n";
    return ss.str(); // 此处根据你的文件截取了关键行
}

std::string EnergyCard::generate() const {
    std::stringstream ss;
    ss << "*CONTROL_ENERGY\n$#    hgen      rwen    slnten     rylen\n";
    ss << "         1         2         2         1\n";
    return ss.str();
}

std::string TerminationCard::generate() const {
    std::stringstream ss;
    ss << "*CONTROL_TERMINATION\n$#  endtim    endcyc     dtmin    endeng    endmas     nosol\n";
    ss << DynaFormat::F10(endtim) << "         0       0.0" << DynaFormat::E10(1.0e8) << "         0         0\n";
    return ss.str();
}

std::string TimestepCard::generate() const {
    std::stringstream ss;
    ss << "*CONTROL_TIMESTEP\n$#  dtinit    tssfac      isdo    tslimt     dt2ms      lctm     erode     ms1st\n";
    ss << "       0.0      0.67         0       0.0       0.0         0         0         0\n";
    return ss.str();
}