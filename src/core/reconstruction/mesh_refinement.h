#ifndef MESH_REFINEMENT_H
#define MESH_REFINEMENT_H
#include "src/core/common/common_types.h"

namespace Mesh_Refinement {

inline void Split( Hexahedron initial_hex, std::vector<MeshPoint>& points ){

    std::vector< MeshPoint > new_meshpoint;
    QuadFace front, back;

    for(int i=0; i<4; i++)front[i] = initial_hex[i];
    for(int i=4; i<8; i++)back[i] = initial_hex[i];

    new_meshpoint.push_back({ Vector3( (points[front[0]].pos.x() + points[front[2]].pos.x() ) / 2,
            (points[front[0]].pos.y() + points[front[2]].pos.y() ) / 2,
            (points[front[0]].pos.z() + points[front[2]].pos.z() ) / 2),
                            6, {-1,-1,-1,-1,-1,-1}
     });

    new_meshpoint.push_back({ Vector3( (points[back[0]].pos.x() + points[back[2]].pos.x() ) / 2,
            (points[back[0]].pos.y() + points[back[2]].pos.y() ) / 2,
            (points[back[0]].pos.z() + points[back[2]].pos.z() ) / 2),
                            6, {-1,-1,-1,-1,-1,-1}
     });
}

}

#endif // MESH_REFINEMENT_H
