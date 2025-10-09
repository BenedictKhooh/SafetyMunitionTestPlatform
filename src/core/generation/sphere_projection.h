#ifndef SPHERE_PROJECTION_H
#define SPHERE_PROjection_H

#include "core/common/common_types.h"
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

/**
 * @struct MeshPoint
 * @brief Represents a point in the mesh with its position and connectivity constraint.
 */
//struct MeshPoint {
//    Vector3 pos;
//    int required_neighbors;
//};

namespace SphereProjection {


inline std::map<MeshPoint, int, MeshPointComparator> MappingMeshVector(std::vector<MeshPoint> meshpoint_vector){

    std::map<MeshPoint, int, MeshPointComparator> return_map;
    int index = 0;
    for (int i =0; (unsigned long long)i<meshpoint_vector.size(); i++){

        index++;

        auto insert_result = return_map.insert({meshpoint_vector[i], i});
        if (!insert_result.second){

            qDebug() << "Failed map insert at:";
            qDebug() << i;
            qDebug() << meshpoint_vector[i].pos.x() << meshpoint_vector[i].pos.y() << meshpoint_vector[i].pos.z();

        }
    }
    return return_map;
}

inline std::vector<int> CountNeighbour( int index, std::vector<MeshPoint>& points ){

    std::vector<int> neighbour_list = points[index].neighbor_indices;
    std::vector<int> return_vector;

    for (const auto & it: neighbour_list){

        if (it == -1)continue;
        if ( points[it].required_neighbors == 6 )continue;

        return_vector.push_back(it);
    }
    return return_vector;
}

/**
     * @brief Generates a complete spherical mesh by projecting an existing cube grid outwards.
     * @param existing_cube_points The pre-generated point cloud for the inner cube.
     * @param sphereRadius The radius of the outer spherical boundary.
     * @param radialDivisions The number of cell layers to create between the cube and the sphere.
     * @return A vector of MeshPoint representing the complete, deduplicated spherical region mesh.
     */
    inline std::vector<MeshPoint> generateSphericalMeshFromCube( std::vector<MeshPoint>& existing_cube_points, float sphereRadius, int radialDivisions) {

        std::vector<MeshPoint> cube_grids = existing_cube_points;
        // --- Calculate the center of the input cube grid ---

        Vector3 cubeCenter = CubicBlock::cube_center_point( existing_cube_points );
        std::vector<MeshPoint> final_points;
        // Use a map to handle duplicates and store the final point data. The key is a string representation of the coordinate.

        std::map<Vector3, MeshPoint, Vector3Comparator> unique_points_map;
        std::map<std::vector<int>, MeshPoint> sphere_connection_map;//Key is cube grid index and path index, value is the pos of the newly generated point
        std::map<int, MeshPoint> sphere_cube_conn_map;
        std::vector<int> original_boundry;

        // --- Stage 1: Process all points from the initial cube grid ---

        int index = 0;
        for ( auto& p_orig : existing_cube_points) {

            if (p_orig.required_neighbors == 6){
                index ++;
                continue;}

            Vector3 relativePos = p_orig.pos - cubeCenter;

            // --- Step A: Project the point outwards to create the shell ---
            Vector3 direction = relativePos.normalized();
            if (direction.isNull()) { // Skip the center point (0,0,0) if it exists.
                 // The center point is special and has no direction to project.
                 // We add it as-is, assuming it's an internal point.
                 Vector3 key = {0.000000,0.000000,0.000000};
                 if(unique_points_map.find(key) == unique_points_map.end()){
                     unique_points_map[key] = { relativePos, 6, {-1,-1,-1,-1,-1,-1} };
                 }
                 continue;
            }
            Vector3 p_sphere = direction * sphereRadius;

            // --- Step B: Generate points along the projection path (from original to sphere) ---
            Vector3 path_vector = p_sphere - p_orig.pos;

            // The full path includes the original point, intermediate points, and the final sphere point.
            // Total points on this path = radialDivisions + 2. Total segments = radialDivisions + 1.
            int segments = radialDivisions + 1;
            original_boundry.push_back(index);

            for (int l = 1; l <= segments; ++l) {

                float t = (float)l / segments;
                Vector3 new_point_pos = p_orig.pos + t * path_vector ;

                // --- Step C: Determine the correct neighbor count based on your logic ---
                int neighbors_count;
                if ( l == segments ) {
                    // This is a point on the final spherical surface. Its connectivity
                    // (corner, edge, face) is inherited from the original cube point.
                    neighbors_count = p_orig.required_neighbors + 1;
                } else {
                    // This is an original point from the cube (l=0) or an intermediate point.
                    // All are now considered internal to the full spherical domain and should have 6 neighbors.
                    neighbors_count = 6;//p_orig.required_neighbors +1;
                }
                sphere_connection_map.insert({ {index,l}, {new_point_pos, neighbors_count, {-1, -1, -1, -1, -1, -1}}});

                // Use a key to handle points that are generated multiple times (shared vertices/edges).
                Vector3 key = new_point_pos;
                // Add or update the point in the map. Updating is important because an internal point
                // from the cube (like a face point) might be processed before a corner point,
                // and we need to ensure the correct neighbor count is assigned.
                unique_points_map[key] = { new_point_pos, neighbors_count, {-1, -1, -1, -1, -1, -1} };
                cube_grids.push_back({new_point_pos, neighbors_count, {-1, -1, -1, -1, -1, -1}});

                if ( l == 1 ){
                    sphere_cube_conn_map.insert( {index, {new_point_pos, neighbors_count, {-1, -1, -1, -1, -1, -1}}} );
                };
            }
//            cube_grids[index].required_neighbors ++;

            index++;
        }

        std::map<MeshPoint, int, MeshPointComparator>grid_map = SphereProjection::MappingMeshVector(cube_grids) ;

        // --- Stage 2: Establish connections between spheric grids ---

        for (const auto& pair : sphere_connection_map){

            if (pair.second.neighbor_indices.size() == 0)continue;

            /** find front, back, up, down, left and right four neighbouring grids
             * fornt: path +1; back: path -1;
             * find index grid's four neighbouring grids which has less than 6 neighbouring grids, meaning that it's on the boundary
             **/

            int index = pair.first[0]; int path = pair.first[1];
            MeshPoint current_meshpoint = pair.second;

            if ( path!= radialDivisions + 1 ){

                std::vector<int> front = {index, path+1};
                MeshPoint front_mesh = sphere_connection_map[ front ];
                int front_index = grid_map[front_mesh];
                current_meshpoint.neighbor_indices[0] = front_index;
            }
            if ( path!= 1 ){

                 std::vector<int> back = {index, path-1};
                 MeshPoint back_mesh = sphere_connection_map[ back ];
                 int back_index = grid_map[back_mesh];
                 current_meshpoint.neighbor_indices[1] = back_index;
            }
            if ( path == 1 ){

                current_meshpoint.neighbor_indices[1] = index;//back
            }

            std::vector<int> neighbour_list = SphereProjection::CountNeighbour( index, cube_grids );
            int neighbour_list_size = neighbour_list.size();
            std::vector<int> neighbouring_point_list;

            for ( const auto& it : neighbour_list ){

                MeshPoint neighbouring_point = sphere_connection_map[{ it, path}];
                int neighbouring_point_index = grid_map[neighbouring_point];

                neighbouring_point_list.push_back(neighbouring_point_index);
            }

            for ( int index = 2; index < neighbour_list_size + 2; index ++ ){

                current_meshpoint.neighbor_indices[index] = neighbouring_point_list[index-2];
            }

            int current_index = grid_map[ current_meshpoint ];
            cube_grids[ current_index ] = current_meshpoint;//insert current point into the final grids vector

        }

        // --- Stage 3: Establish connections between spheric grids and cubic grids ---

        for (const auto& index : original_boundry){

            cube_grids[ index ].required_neighbors++;
        }

        for (const auto& pair : sphere_cube_conn_map){

            int index = pair.first;
            MeshPoint current_meshpoint = pair.second;

            cube_grids[index].neighbor_indices[cube_grids[index].required_neighbors-1] = grid_map[current_meshpoint];
        }
       //  --- Stage 4: Return grids for the whole domain ---
        return cube_grids;
    }

} // namespace SphereProjection

#endif
