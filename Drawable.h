// Drawable.h
#include "src/core/common/common_types.h"
#pragma once
#include <vector>

struct DrawPointsCmd {
    std::vector<MeshPoint> points;
    float size = 2.0f;
};

struct DrawLinesCmd {
    std::vector<Vector3> points; // p0,p1,p2,p3... => (p0,p1),(p2,p3)
    float width = 1.0f;
};

struct DrawCommand {
    enum Type {
        Points,
        Lines
    } type;

    DrawPointsCmd pointsCmd;
    DrawLinesCmd  linesCmd;
};

