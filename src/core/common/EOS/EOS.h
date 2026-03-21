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

    // 🌟 字典构造函数
    EOSCard(int id, const std::string& type, const std::map<std::string, double>& params) {
        eosid = id;
        keyword = type;
        p = params;
    }

    // 🌟 核心多态方法实现
    std::string to_string() const override {
        char buf[1024] = { 0 };
        auto getVal = [&](const std::string& key) { return p.count(key) ? p.at(key) : 0.0; };

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
        else if (keyword == "JWL") {
            snprintf(buf, sizeof(buf),
                "*EOS_JWL\n"
                "$#   eosid         a         b        r1        r2     omega        e0        v0\n"
                "%10d%10.4E%10.4E%10.4f%10.4f%10.4f%10.4E       1.0\n",
                eosid, getVal("jwl_a"), getVal("jwl_b"), getVal("jwl_r1"), getVal("jwl_r2"), getVal("jwl_omega"), getVal("jwl_e0")
            );
        }

        return std::string(buf);
    }
};
#endif