#pragma once
#include "src/core/common/KeywordCard.h"
#include <string>

// --- Johnson-Cook (带完整 D1-D5 损伤参数) ---
class MaterialJohnsonCook : public KeywordCard {
public:
    int mid; std::string title;
    double ro, g, e, pr;
    double a, b, n, c, m, tm, tr, epso;
    double cp, pc, spall, it, d1, d2, d3, d4, d5;
    MaterialJohnsonCook(int id, std::string t);
    std::string generate() const override;
};

// --- Lee-Tarver 状态方程 (用于炸药起爆) ---
class EOSLeeTarver : public KeywordCard {
public:
    int eosid;
    double a, b, xp1, xp2, frer, g, r1, r2, r3, r5, r6, fmxig, freq, grow1, em;
    double ar1, es1, cvp, cvr, eetal, ccrit, enq, tmp0, grow2, ar2, es2, en, fmxgr, fmngr;
    EOSLeeTarver(int id);
    std::string generate() const override;
};

// --- Plastic Kinematic (用于壳体简单塑性) ---
class MatPlasticKinematic : public KeywordCard {
public:
    int mid; double ro, e, pr, sigy, etan, beta;
    MatPlasticKinematic(int id, double r, double elastic, double p_ratio, double yield, double tangent);
    std::string generate() const override;
};

// --- Gruneisen 状态方程 ---
class EOSGruneisen : public KeywordCard {
public:
    int eosid; double c, s1, s2, s3, gamao, a, e0, v0;
    EOSGruneisen(int id, double C_val, double S1_val, double G_val, double A_val);
    std::string generate() const override;
};

// --- 侵蚀失效准则 ---
class MatAddErosion : public KeywordCard {
public:
    int mid; double effeps, mxeps;
    MatAddErosion(int m_id, double effective_strain, double max_strain);
    std::string generate() const override;
};