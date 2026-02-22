// MeshUtils.h
#pragma once
#include <vector>
#include "core/common/common_types.h"

std::vector<Vector3> buildHexWireframe(
    const std::vector<MeshPoint>& points,
    const std::vector<Hexahedron>& hexes
);
