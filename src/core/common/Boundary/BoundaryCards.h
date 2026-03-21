#ifndef BOUNDARYCARDS_H
#define BOUNDARYCARDS_H

#include <vector>
#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

// 记录单个约束的数据结构
struct SPC {
    int nsid; // 节点集或 Part ID
    int dofx, dofy, dofz, dofrx, dofry, dofrz; // 6个自由度 (1代表固定，0代表自由)
};

// 🌟 继承自 KeywordCard 的边界条件卡片
struct SpcSetCard : public KeywordCard {
    std::vector<SPC> constraints;

    SpcSetCard() = default;

    // 添加约束的方法
    void addConstraint(int id, int x, int y, int z, int rx, int ry, int rz) {
        constraints.push_back({ id, x, y, z, rx, ry, rz });
    }

    // 🌟 核心多态方法：生成标准的 K 文件文本
    std::string to_string() const override {
        if (constraints.empty()) return "";

        std::string res = "*BOUNDARY_SPC_SET\n";
        char buf[256];
        for (const auto& c : constraints) {
            snprintf(buf, sizeof(buf),
                "$#    nsid       cid      dofx      dofy      dofz     dofrx     dofry     dofrz\n"
                "%10d         0%10d%10d%10d%10d%10d%10d\n",
                c.nsid, c.dofx, c.dofy, c.dofz, c.dofrx, c.dofry, c.dofrz);
            res += buf;
        }
        return res;
    }
};

#endif // BOUNDARYCARDS_H