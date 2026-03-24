#ifndef INITIALCONDITIONS_H
#define INITIALCONDITIONS_H
#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct InitialVelocityGenerationCard : public KeywordCard {
    int nsid;
    int styp;
    double vx, vy, vz;
    double omega;

    // 默认构造
    InitialVelocityGenerationCard() = default;

    // 🌟 强类型参数构造
    InitialVelocityGenerationCard(int partId, double x, double y, double z) {
        nsid = partId;
        styp = 2; // Auto: 作用于 Part
        omega = 0.0;
        vx = x; vy = y; vz = z;
    }

    // 🌟 核心多态方法实现
    std::string to_string() const override {
        char buf[2048];
        snprintf(buf, sizeof(buf),
            "*INITIAL_VELOCITY_GENERATION\n"
            "$#nsid/pid      styp     omega        vx        vy        vz      ivat      icid\n"
            "%10d%10d       0.0%10.4f%10.4f%10.4f         0         0\n"
            "$#      xc        yc        zc        nx        ny        nz     phase    irigid\n"
            "       0.0       0.0       0.0       0.0       0.0       0.0         0         0\n",
            nsid, styp, vx, vy, vz);
        return std::string(buf);
    }
};
#endif