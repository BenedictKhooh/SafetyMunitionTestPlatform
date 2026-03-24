#ifndef PARTCARD_H
#define PARTCARD_H
#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

struct PartCard : public KeywordCard {
    int pid = 1;
    int secid = 1;
    int mid = 1;
    int eosid = 1;
    std::string heading;

    PartCard() = default;

    // 🌟 强类型参数构造
    PartCard(int partId, const std::string& name) {
        pid = partId;
        secid = partId;
        mid = partId;
        eosid = 0;      // 默认为0，如果后期有EOS，由MainWindow智能指针直接修改
        heading = name;
    }

    // 🌟 核心多态方法实现
    std::string to_string() const override {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "*PART\n"
            "$#                                                                         title\n"
            "%s\n"
            "$#     pid     secid       mid     eosid      hgid      grav    adpopt      tmid\n"
            "%10d%10d%10d%10d         0         0         0         0\n",
            heading.empty() ? "Part_Auto" : heading.c_str(),
            pid, secid, mid, eosid
        );
        return std::string(buf);
    }
};
#endif