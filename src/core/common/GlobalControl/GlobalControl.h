#ifndef CONTROLCARDS_H
#define CONTROLCARDS_H

#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct GlobalControlCard : public KeywordCard {
    double endtim; // 结束时间
    double tssfac; // 时间步缩放因子
    double dt;     // D3PLOT 和 ASCII 数据库输出间隔

    GlobalControlCard() = default;

    // 🌟 构造函数：只接收 UI 上需要填写的 3 个变量
    GlobalControlCard(double end_time, double ts_scale, double plot_freq) {
        endtim = end_time;
        tssfac = ts_scale;
        dt = plot_freq;
    }

    // 🌟 核心多态方法：把 control_mate.k 里的所有默认控制参数一次性打包！
    std::string to_string() const override {
        char buf[4096];
        snprintf(buf, sizeof(buf),
            "*CONTROL_BULK_VISCOSITY\n"
            "$#      q1        q2      type     btype    tstype      \n"
            "       1.5      0.06         1         0         0\n"
            "*CONTROL_CONTACT\n"
            "$#  slsfac    rwpnal    islchk    shlthk    penopt    thkchg     orien    enmass\n"
            "       0.2       0.0         2         0         1         1         1         0\n"
            "$#  usrstr    usrfrc     nsbcs    interm     xpene     ssthk      ecdt   tiedprj\n"
            "         0         0         0         0       4.0         0         0         0\n"
            "$#   sfric     dfric       edc       vfc        th     th_sf    pen_sf      \n"
            "       0.0       0.0       0.0       0.0       0.0       0.0       0.0\n"
            "$#  ignore    frceng   skiprwg    outseg   spotstp   spotdel   spothin       \n"
            "         0         1         0         0         2         0       0.0\n"
            "$#    isym    nserod    rwgaps    rwgdth     rwksf      icov    swradf    ithoff\n"
            "         0         0         1       0.0       1.0         0       0.0         0\n"
            "*CONTROL_ENERGY\n"
            "$#    hgen      rwen    slnten     rylen\n"
            "         2         2         1         1\n"
            "*CONTROL_HOURGLASS\n"
            "$#     ihq        qh\n"
            "         4       0.1\n"
            "*CONTROL_OUTPUT\n"
            "$#   npopt    neecho    nrefup    iaccop     opifs    pelopt    txsolv\n"
            "         1         3         1         0         0         1         1\n"
            "*CONTROL_TERMINATION\n"
            "$#  endtim    endcyc     dtmin    endeng    endmas     nosof\n"
            "%10.4f         0       0.0       0.0       0.0         0\n"
            "*CONTROL_TIMESTEP\n"
            "$#  dtinit    tssfac      isdo    tslimt     dt2ms      lctm     erode     ms1st\n"
            "       0.0%10.4f         0       0.0 -1.20E-04         0         0         0\n"
            "*DATABASE_BINARY_D3PLOT\n"
            "$#      dt      lcdt      beam     npltc    psetid      \n"
            "%10.4E         0         0         0         0\n"
            "*DATABASE_EXTENT_BINARY\n"
            "$#   neiph     neips    maxint    strflg    sigflg    epsflg    rltflg    engflg\n"
            "         0         0         3         1         1         1         1         1\n"
            "$#  cmpflg    ieverp    beamip     dcomp      shge     stssz    n3thdt   ialemat\n"
            "         0         0         0         0         0         0         0         0\n"
            "$# nintsld   pkp_sen      sclp     hspid     msscl     therm    intout    nodout\n"
            "         0         0         0         0         0         0         0         0\n"
            "*DATABASE_GLSTAT\n%10.4E         0         0         0         0         0\n"
            "*DATABASE_MATSUM\n%10.4E         0         0         0         0         0\n"
            "*DATABASE_RCFORC\n%10.4E         0         0         0         0         0\n"
            "*DATABASE_SLEOUT\n%10.4E         0         0         0         0         0\n",
            endtim, tssfac, dt, dt, dt, dt, dt
        );
        return std::string(buf);
    }
};

#endif // CONTROLCARDS_H#pragma once
