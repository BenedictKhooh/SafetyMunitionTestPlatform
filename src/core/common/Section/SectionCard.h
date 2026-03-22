#ifndef SECTIONCARD_H
#define SECTIONCARD_H

#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct SectionCard : public KeywordCard {
    int secid;
    std::string type; // "SOLID" 或 "SHELL"
    int elform;       // 单元算法

    SectionCard() = default;

    // 构造函数：接收 UI 传来的 ID、类型和算法编号
    SectionCard(int id, const std::string& t, int ef) : secid(id), type(t), elform(ef) {}

    // 核心多态方法：生成 LS-DYNA 严格格式
    std::string to_string() const override {
        char buf[1024];

        if (type == "SOLID") {
            // 完美复刻你算例中的 *SECTION_SOLID 格式
            snprintf(buf, sizeof(buf),
                "*SECTION_SOLID\n"
                "$#   secid    elform       aet\n"
                "%10d%10d         0\n",
                secid, elform);
        }
        else if (type == "SHELL") {
            // 补充 SHELL 的基本默认格式 (厚度默认设为1.0，防止报错)
            snprintf(buf, sizeof(buf),
                "*SECTION_SHELL\n"
                "$#   secid    elform    shrfac      nip    propt   qr/irid     icomp     setyp\n"
                "%10d%10d       1.0         2         1         0         0         1\n"
                "$#      t1        t2        t3        t4      nloc     marea      idof    edgset\n"
                "       1.0       1.0       1.0       1.0       0.0       0.0       0.0         0\n",
                secid, elform);
        }

        return std::string(buf);
    }
};

#endif // SECTIONCARD_H