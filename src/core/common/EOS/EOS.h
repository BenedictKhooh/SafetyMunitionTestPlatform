#ifndef EOSCARD_H
#define EOSCARD_H

#include <string>
#include <map>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct EOSCard : public KeywordCard {
    int eosid;
    std::string keyword;
    std::map<std::string, double> p;

    EOSCard() = default;

    // 🌟 字典构造函数：外部只需传入 ID、EOS类型名称 和 包含参数的字典
    EOSCard(int id, const std::string& type, const std::map<std::string, double>& params) {
        eosid = id;
        keyword = type;
        p = params;
    }

    // 🌟 核心多态方法实现：按 LS-DYNA 严格的 10 字符宽度输出
    std::string to_string() const override {
        // 增加 buffer 的大小以防高级 EOS 参数过多
        char buf[2048] = { 0 };

        // 强大的取值闭包：字典里有的就取出来，没有的自动赋 0.0，极大地提高了容错率
        auto getVal = [&](const std::string& key) { return p.count(key) ? p.at(key) : 0.0; };

        // 1. Mie-Gruneisen (金属/固体标准抗冲击方程)
        if (keyword == "GRUNEISEN") {
            snprintf(buf, sizeof(buf),
                "*EOS_GRUNEISEN\n"
                "$#   eosid         c        s1        s2        s3     gamao         a        e0\n"
                "%10d%10.4E%10.4f       0.0       0.0%10.4f       0.0       0.0\n"
                "$#      v0\n"
                "       0.0\n",
                eosid, getVal("gr_c"), getVal("gr_s1"), getVal("gr_gamao")
            );
        }
        // 2. JWL (常规理想高能炸药爆轰产物)
        else if (keyword == "JWL") {
            snprintf(buf, sizeof(buf),
                "*EOS_JWL\n"
                "$#   eosid         a         b        r1        r2     omega        e0        v0\n"
                "%10d%10.4E%10.4E%10.4f%10.4f%10.4f%10.4E       1.0\n",
                eosid, getVal("jwl_a"), getVal("jwl_b"), getVal("jwl_r1"), getVal("jwl_r2"), getVal("jwl_omega"), getVal("jwl_e0")
            );
        }
        // ==============================================================
        // 👇 以下为新增的三大高速冲击物理常用 EOS
        // ==============================================================

        // 3. Linear Polynomial (线性多项式，常用于水、空气、非反应性气体)
        else if (keyword == "LINEAR_POLYNOMIAL") {
            snprintf(buf, sizeof(buf),
                "*EOS_LINEAR_POLYNOMIAL\n"
                "$#   eosid        c0        c1        c2        c3        c4        c5        c6\n"
                "%10d%10.4E%10.4E%10.4E%10.4E%10.4E%10.4E%10.4E\n"
                "$#      e0        v0\n"
                "%10.4E%10.4E\n",
                eosid, getVal("lp_c0"), getVal("lp_c1"), getVal("lp_c2"), getVal("lp_c3"),
                getVal("lp_c4"), getVal("lp_c5"), getVal("lp_c6"), getVal("lp_e0"), getVal("lp_v0")
            );
        }
        // 4. Tillotson (提洛森，用于超高速撞击导致金属相变气化，如太空碎片防护)
        else if (keyword == "TILLOTSON") {
            snprintf(buf, sizeof(buf),
                "*EOS_TILLOTSON\n"
                "$#   eosid         a         b     omega        e0        v0        v1        v2\n"
                "%10d%10.4E%10.4E%10.4f%10.4E%10.4f       0.0       0.0\n"
                "$#   alpha      beta\n"
                "%10.4f%10.4f\n",
                eosid, getVal("til_a"), getVal("til_b"), getVal("til_omega"), getVal("til_e0"),
                getVal("til_v0"), getVal("til_alpha"), getVal("til_beta")
            );
        }
        // 5. Ignition and Growth (点火与生长，用于模拟炸药被破片撞击起爆的动态化学反应过程)
        else if (keyword == "IGNITION_AND_GROWTH_OF_REACTION_IN_HE") {
            snprintf(buf, sizeof(buf),
                "*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE\n"
                "$#   eosid         a         b       xp1       xp2      frer         g        r1\n"
                "%10d%10.4E%10.4E%10.4E%10.4E%10.4E%10.4E%10.4f\n"
                "$#      r2        r3        r5        r6     fmxig      freq     grow1        em\n"
                "%10.5f%10.4E%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f\n"
                "$#     ar1       es1       cvp       cvr     eetal     ccrit       enq      tmp0\n"
                "%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f\n"
                "$#   grow2       ar2       es2        en     fmxgr     fmngr\n"
                "%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f\n",
                eosid, getVal("ig_a"), getVal("ig_b"), getVal("ig_xp1"), getVal("ig_xp2"),
                getVal("ig_frer"), getVal("ig_g"), getVal("ig_r1"), getVal("ig_r2"),
                getVal("ig_r3"), getVal("ig_r5"), getVal("ig_r6"), getVal("ig_fmxig"),
                getVal("ig_freq"), getVal("ig_grow1"), getVal("ig_em"), getVal("ig_ar1"),
                getVal("ig_es1"), getVal("ig_cvp"), getVal("ig_cvr"), getVal("ig_eetal"),
                getVal("ig_ccrit"), getVal("ig_enq"), getVal("ig_tmp0"), getVal("ig_grow2"),
                getVal("ig_ar2"), getVal("ig_es2"), getVal("ig_en"), getVal("ig_fmxgr"),
                getVal("ig_fmngr")
            );
        }

        return std::string(buf);
    }
};

#endif // EOSCARD_H