#pragma once
#include "src/core/common/KeywordCard.h"
#include "src/core/common/DynaFormat.h"
#include <sstream>

class InitialVelocityCard : public KeywordCard {
public:
    int id; // pid or nsid
    double vx;
    InitialVelocityCard(int target_id, double vel_x) : id(target_id), vx(vel_x) {}
    std::string generate() const override {
        std::stringstream ss;
        ss << "*INITIAL_VELOCITY_GENERATION\n";
        ss << "$#nsid/pid      styp     omega        vx        vy        vz     ivatn      icid\n";
        ss << DynaFormat::I10(id) << "         2       0.0" << DynaFormat::F10(vx) << "       0.0       0.0         0         0\n";
        ss << "$#      xc        yc        zc        nx        ny        nz     phase    irigid\n";
        ss << "       0.0       0.0       0.0       0.0       1.0       0.0         0         0\n";
        return ss.str();
    }
};