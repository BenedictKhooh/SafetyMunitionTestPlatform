#ifndef CUBIC_BLOCK_H
#define CUBIC_BLOCK_H
#include "core/common/common_types.h"
#include "core/reconstruction/reconstruction_engine.h"
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <algorithm>
#include <limits>

// Qt Includes for QVector3D and qDebug
// In your .pro file, you will need: QT += core
#include <QVector3D>
#include <QString>
#include <QDebug>

namespace CubicBlock {

inline std::vector<MeshPoint> building_basic_cubic_brick(int x_division, int y_division, int z_division, float x, float y, float z, float x_scale, float y_scale, float z_scale){

    std::vector<MeshPoint> block_points;

    for (int k = 0; k < z_division + 1; ++k) {
        for (int j = 0; j < y_division + 1; ++j) {
            for (int i = 0; i < x_division + 1; ++i) {
                // Determine if the point is on a boundary
                bool on_x = (i == 0 || i == x_division);
                bool on_y = (j == 0 || j == y_division);
                bool on_z = (k == 0 || k == z_division);

                int boundary_count = (int)on_x + (int)on_y + (int)on_z;
                int neighbors = 0;

                if (boundary_count == 3) { // Corner
                    neighbors = 3;
                } else if (boundary_count == 2) { // Edge
                    neighbors = 4;
                } else if (boundary_count == 1) { // Face
                    neighbors = 5;
                } else { // Internal
                    neighbors = 6;
                }

                block_points.push_back({
                    Vector3( (float(i)/x_division - 0.5), (float(j)/y_division - 0.5), (float(k)/z_division - 0.5) ),
                                       neighbors, {-1,-1,-1,-1,-1,-1}
                });
            }
        }
    }
    AdjacencyGraph initial_adj = ReconstructionEngine::buildAdjacencyGraph( block_points );
    //return a 1*1*1 cube with (0,0,0) as its centter

    for ( auto& point : block_points ){

        point.pos.setX( point.pos.x() * x_scale+ x );
        point.pos.setY( point.pos.y() * y_scale+ y );
        point.pos.setZ( point.pos.z() * z_scale+ z );
    }

    return block_points;

}

inline std::vector<MeshPoint> building_basic_cubic_brick(int x_division, int y_division, int z_division, float x, float y, float z) {

    std::vector<MeshPoint> block_points;

    for (int k = 0; k < z_division + 1; ++k) {
        for (int j = 0; j < y_division + 1; ++j) {
            for (int i = 0; i < x_division + 1; ++i) {
                // Determine if the point is on a boundary
                bool on_x = (i == 0 || i == x_division);
                bool on_y = (j == 0 || j == y_division);
                bool on_z = (k == 0 || k == z_division);

                int boundary_count = (int)on_x + (int)on_y + (int)on_z;
                int neighbors = 0;

                if (boundary_count == 3) { // Corner
                    neighbors = 3;
                }
                else if (boundary_count == 2) { // Edge
                    neighbors = 4;
                }
                else if (boundary_count == 1) { // Face
                    neighbors = 5;
                }
                else { // Internal
                    neighbors = 6;
                }

                block_points.push_back({
                    Vector3(float(i) / x_division - 0.5, float(j) / y_division - 0.5, float(k) / z_division - 0.5),
                                       neighbors, {-1,-1,-1,-1,-1,-1}
                    });
            }
        }
    }
    AdjacencyGraph initial_adj = ReconstructionEngine::buildAdjacencyGraph(block_points);
    //return a 1*1*1 cube with (0,0,0) as its centter

    for (auto& point : block_points) {

        point.pos.setX(point.pos.x() + x);
        point.pos.setY(point.pos.y() + y);
        point.pos.setZ(point.pos.z() + z);
    }

    return block_points;
}

inline Vector3 cube_center_point ( std::vector <MeshPoint> mesh_points ){

    // 1. 处理输入为空的边缘情况
        if (mesh_points.empty()) {
            return Vector3(0, 0, 0);
        }

        // 2. 初始化边界
        Vector3 minBounds(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        Vector3 maxBounds(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

        // 3. 遍历所有点来找到边界框
        for (const auto& point : mesh_points) {
            minBounds.setX(std::min(minBounds.x(), point.pos.x()));
            minBounds.setY(std::min(minBounds.y(), point.pos.y()));
            minBounds.setZ(std::min(minBounds.z(), point.pos.z()));

            maxBounds.setX(std::max(maxBounds.x(), point.pos.x()));
            maxBounds.setY(std::max(maxBounds.y(), point.pos.y()));
            maxBounds.setZ(std::max(maxBounds.z(), point.pos.z()));
        }

        // 4. 计算并返回中心点
        // 公式: 中心 = 最小角点 + (最大角点 - 最小角点) / 2
        return minBounds + (maxBounds - minBounds) * 0.5f;
}

}

#endif // CUBIC_BLOCK_H
