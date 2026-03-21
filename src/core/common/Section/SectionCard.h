#ifndef SECTIONCARD_H
#define SECTIONCARD_H
#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct SectionCard : public KeywordCard {
    int secid;
    std::string keyword; // "SOLID" 或者是 "SHELL"

    SectionCard() = default;

    // 🌟 强类型参数构造
    SectionCard(int id, const std::string& type) {
        secid = id;
        keyword = type;
    }

    // 🌟 核心多态方法实现
    std::string to_string() const override {
        char buf[512];
        if (keyword == "SOLID") {
            snprintf(buf, sizeof(buf),
                "*SECTION_SOLID\n"
                "$#   secid    elform      aet\n"
                "%10d         1        0\n",
                secid
            );
        }
        else if (keyword == "SHELL") {
            snprintf(buf, sizeof(buf),
                "*SECTION_SHELL\n"
                "$#   secid    elform      shrf       nip     propt   qr/irid     icomp     setyp\n"
                "%10d         2       1.0         2         1         0         0         1\n"
                "$#      t1        t2        t3        t4      nloc     marea      idof    edgset\n"
                "       1.0       1.0       1.0       1.0       0.0       0.0       0.0       0.0\n",
                secid
            );
        }
        return std::string(buf);
    }
};
#endif