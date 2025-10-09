#ifndef INSTANCE_BUILDER_H
#define INSTANCE_BUILDER_H
#include "core/common/common_types.h"
#include "core/reconstruction/reconstruction_engine.h"
#include "core/generation/cubic_block.h"

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

namespace InstanceBuilder{

inline Instance CubeBuilder(QString instance_name, float x, float y, float z, Vector3 center_point){

    Instance* cube = new Instance;
    cube->instance_name = instance_name;
    std::vector<MeshPoint> instance_meshpoint = CubicBlock::building_basic_cubic_brick(center_point, x, y, z );

    cube->instance_points = instance_meshpoint;
    cube->instance_adjacenctgraph = ReconstructionEngine::buildAdjacencyGraph(cube->instance_points);
    cube->instance_quadface_list = ReconstructionEngine::findValidFaces( cube->instance_points, cube->instance_adjacenctgraph );
    cube->instance_hexahedron_list = ReconstructionEngine::buildHexahedra( cube->instance_quadface_list, cube->instance_adjacenctgraph );

    return *cube;
}

inline Instance SphereBuilder(QString instance_name ){

    Instance* sphere = new Instance;
    sphere->instance_name = instance_name;

}

inline Instance CylinderBuilder(QString instance_name ){

    Instance* cylinder  = new Instance;
    cylinder->instance_name = instance_name;
}

}

#endif // INSTANCE_BUILDER_H
