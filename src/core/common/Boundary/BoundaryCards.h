#ifndef BOUNDARYCARDS_H
#define BOUNDARYCARDS_H

#include <vector>
#include <string>
#include <cstdio>
#include "src/core/common/KeywordCard.h"

// ==========================================
// 🌟 新增：节点集合卡片 (为边界条件提供节点集)
// ==========================================
struct SetNodeListCard : public KeywordCard {
    int sid;             // 集合 ID
    std::string title;   // 集合名称
    std::vector<int> nodeIds;

    SetNodeListCard(int id, const std::string& t) : sid(id), title(t) {}

    void addNode(int nid) {
        nodeIds.push_back(nid);
    }

    // 沿用你的 snprintf 核心逻辑，保证对齐风格一致
    std::string to_string() const override {
        if (nodeIds.empty()) return "";

        std::string res = "*SET_NODE_LIST_TITLE\n" + title + "\n";
        char buf[256];
        snprintf(buf, sizeof(buf), "%10d\n", sid);
        res += buf;

        int count = 0;
        for (int nid : nodeIds) {
            snprintf(buf, sizeof(buf), "%10d", nid);
            res += buf;
            count++;
            if (count % 8 == 0) res += "\n"; // 满 8 个换行
        }
        if (count % 8 != 0) res += "\n";

        return res;
    }
};

// 记录单个约束的数据结构 (原封不动)
struct SPC {
    int nsid;
    int dofx, dofy, dofz, dofrx, dofry, dofrz;
};

// 🌟 继承自 KeywordCard 的边界条件卡片 (原封不动)
struct SpcSetCard : public KeywordCard {
    std::vector<SPC> constraints;

    SpcSetCard() = default;

    void addConstraint(int id, int x, int y, int z, int rx, int ry, int rz) {
        constraints.push_back({ id, x, y, z, rx, ry, rz });
    }

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