// MeshUtils.cpp
#include "MeshUtils.h"
#include <QDebug>

std::vector<Vector3> buildHexWireframe(
    const std::vector<MeshPoint>& points,
    const std::vector<Hexahedron>& hexes
) {
    std::vector<Vector3> lines;

    static const int hexEdges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    for (const auto& hex : hexes) {
        for (int e = 0; e < 12; ++e) {
            int i0 = hex[hexEdges[e][0]];
            int i1 = hex[hexEdges[e][1]];

            if (i0 >= points.size() || i1 >= points.size()) continue;

            lines.push_back(points[i0].pos);
            lines.push_back(points[i1].pos);

        }
    }

    return lines;
}

std::vector<Hexahedron>
convertHexElements(const std::vector<HexElement>& elems) {
    std::vector<Hexahedron> hexes;
    hexes.reserve(elems.size());

    for (const auto& e : elems) {
        Hexahedron h;
        for (int i = 0; i < 8; ++i)
            h[i] = e.nodeIndices[i];
        hexes.push_back(h);
    }
    return hexes;
}
std::set<int> getSymmetryPlaneNodes(const MeshEntity& entity, char axis) {
    std::set<int> symNodes;
    if (entity.nodes.empty()) return symNodes;

    // 1. 寻找极小值，同时寻找极大值以便计算包围盒尺寸
    double minVal = 1e10;
    double maxVal = -1e10;
    for (const auto& node : entity.nodes) {
        double val = (axis == 'X') ? node.pos.x() :
            (axis == 'Y') ? node.pos.y() : node.pos.z();
        if (val < minVal) minVal = val;
        if (val > maxVal) maxVal = val;
    }

    // 2. 动态计算极度安全的容差
    double span = maxVal - minVal;
    double tol = 1e-6; // 默认基准值

    if (span > 1e-8 && entity.nodes.size() > 1) {
        double approxLayers = std::cbrt(entity.nodes.size());
        double avgElementSize = span / approxLayers;
        tol = avgElementSize * 0.01;
    }

    if (tol < 1e-9) tol = 1e-9;

    // 3. 使用自适应容差进行安全过滤
    for (size_t i = 0; i < entity.nodes.size(); ++i) {
        double val = (axis == 'X') ? entity.nodes[i].pos.x() :
            (axis == 'Y') ? entity.nodes[i].pos.y() : entity.nodes[i].pos.z();

        if (std::abs(val - minVal) < tol) {
            symNodes.insert(i); // 记录相对索引
        }
    }

    return symNodes;
}