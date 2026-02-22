// MeshUtils.cpp
#include "MeshUtils.h"

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
