// MeshUtils.h
#pragma once
#include <vector>
#include <set>
#include "core/common/common_types.h"
#include "src/core/common/common_types.h"
#include "src/core/common/EntityRepository.h"

std::vector<Vector3> buildHexWireframe(
    const std::vector<MeshPoint>& points,
    const std::vector<Hexahedron>& hexes
);

std::set<int> getSymmetryPlaneNodes(const MeshEntity& entity, char axis);