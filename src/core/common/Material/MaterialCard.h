#ifndef MATERIALCARD_H
#define MATERIALCARD_H
#include <string>
#include <map>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct MaterialCard : public KeywordCard {
    int mid;
    std::string keyword;
    std::map<std::string, double> p;

    MaterialCard() = default;

    // 🌟 字典构造函数 (方便 make_shared)
    MaterialCard(int pid, const std::string& type, const std::map<std::string, double>& params) {
        mid = pid;
        keyword = type;
        p = params;
    }

    // 🌟 核心多态方法实现
    std::string to_string() const override {
        char buf[1024] = { 0 };
        auto getVal = [&](const std::string& key) { return p.count(key) ? p.at(key) : 0.0; };

        if (keyword == "JOHNSON_COOK") {
            snprintf(buf, sizeof(buf),
                "*MAT_JOHNSON_COOK\n"
                "$#     mid        ro         g         e        pr     dtf     vp    rateop\n"
                "%10d%10.4E%10.4E       0.3       0.0       0.0       0.0\n"
                "$#       a         b         n         c         m     tmelt      tr     epso\n"
                "%10.4E%10.4E%10.4f%10.4f%10.4f%10.1f       0.0       0.0\n"
                "$#      cp        pc     spall        it        d1        d2        d3        d4\n"
                "       0.0       0.0       0.0       0.0       0.0       0.0       0.0       0.0\n",
                mid, getVal("ro"), getVal("g"), getVal("a"), getVal("b"), getVal("n"), getVal("c"), getVal("m"), getVal("tmelt")
            );
        }
        else if (keyword == "PLASTIC_KINEMATIC") {
            snprintf(buf, sizeof(buf),
                "*MAT_PLASTIC_KINEMATIC\n"
                "$#     mid        ro         e        pr      sigy      etan      beta\n"
                "%10d%10.4E%10.4E%10.4f%10.4E%10.4f       1.0\n"
                "$#     src       srp        fs        vp\n"
                "       0.0       0.0       1.0       1.0\n",
                mid, getVal("ro"), getVal("e"), getVal("pr"), getVal("sigy"), getVal("etan")
            );
        }
        else if (keyword == "HIGH_EXPLOSIVE_BURN") {
            snprintf(buf, sizeof(buf),
                "*MAT_HIGH_EXPLOSIVE_BURN\n"
                "$#     mid        ro         d       pcj      beta         k         g   sigmay\n"
                "%10d%10.4E%10.4E%10.4E         0         0         0         0\n",
                mid, getVal("ro"), getVal("d"), getVal("pcj")
            );
        }
        else if (keyword == "ELASTIC_PLASTIC_HYDRO") {
            snprintf(buf, sizeof(buf),
                "*MAT_ELASTIC_PLASTIC_HYDRO\n"
                "$#     mid        ro         g      sigy        eh        pc        fs     charl\n"
                "%10d%10.4E%10.4E%10.4E       0.0%10.4f       0.0       0.0\n"
                "$#    eps1      eps2      eps3      eps4      eps5      eps6      eps7      eps8\n"
                "       0.0       0.0       0.0       0.0       0.0       0.0       0.0       0.0\n"
                "$#    eps9     eps10     eps11     eps12     eps13     eps14     eps15     eps16\n"
                "       0.0       0.0       0.0       0.0       0.0       0.0       0.0       0.0\n"
                "$#     es1       es2       es3       es4       es5       es6       es7       es8\n"
                "       0.0       0.0       0.0       0.0       0.0       0.0       0.0       0.0\n"
                "$#     es9      es10      es11      es12      es13      es14      es15      es16\n"
                "       0.0       0.0       0.0       0.0       0.0       0.0       0.0       0.0\n",
                mid, getVal("ro"), getVal("g"), getVal("sigy"), getVal("pc")
            );
        }

        std::string res = buf;

        // 如果用户在UI上勾选了侵蚀，这里会自动追加侵蚀卡
        if (p.count("mxeps") && p.at("mxeps") > 0.0) {
            char eroBuf[512];
            snprintf(eroBuf, sizeof(eroBuf),
                "*MAT_ADD_EROSION\n"
                "$#     mid      excl    mxpres     mneps    effeps    voleps    numfip       ncs\n"
                "%10d       0.0       0.0       0.0       0.0       0.0       1.0       1.0\n"
                "$#  mnpres     sigp1     sigvm     mxeps     epssh     sigth   impulse    failtm\n"
                "       0.0       0.0       0.0%10.4f       0.0       0.0       0.0       0.0\n",
                mid, p.at("mxeps")
            );
            res += eroBuf;
        }

        return res;
    }
};
#endif