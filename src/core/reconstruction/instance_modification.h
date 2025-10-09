#ifndef INSTANCE_MODIFICATION_H
#define INSTANCE_MODIFICATION_H
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

#include <QQuaternion>

namespace InstanceModification {

inline void DisplacementX(std::vector<MeshPoint> mesh_points, float X){

    for (auto& point : mesh_points){

        point.pos.setX( point.pos.x() + X );

    }
    return;
}

inline void DisplacementY(std::vector<MeshPoint> mesh_points, float Y){

    for (auto& point : mesh_points){

        point.pos.setY( point.pos.y() + Y );

    }
    return;
}

inline void DisplacementZ(std::vector<MeshPoint> mesh_points, float Z){

    for (auto& point : mesh_points){

        point.pos.setZ( point.pos.z() + Z );

    }
    return;
}

inline void Revolve(std::vector<MeshPoint> mesh_points, Vector3& axisPoint, Vector3& axisDirection, float angleDegrees ){

    if (mesh_points.empty()) {
            return;
        }

        // 1. 根据轴和角度创建旋转四元数
        QQuaternion rotation = QQuaternion::fromAxisAndAngle(axisDirection, angleDegrees);

        // 2. 遍历并旋转每一个点
        for (auto& point : mesh_points) {
            // a. 将点平移到以 axisPoint 为原点的坐标系
            Vector3 translatedPoint = point.pos - axisPoint;

            // b. 使用四元数旋转平移后的点
            Vector3 rotatedPoint = rotation.rotatedVector(translatedPoint);

            // c. 将点平移回原来的坐标系
            point.pos = rotatedPoint + axisPoint;
        }

    return;
}


}

#endif // INSTANCE_MODIFICATION_H
