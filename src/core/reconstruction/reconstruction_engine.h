#ifndef RECONSTRUCTION_ENGINE_H
#define RECONSTRUCTION_ENGINE_H

#include "core/common/common_types.h"

#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <QVector3D>
#include <QDebug>
#include <QSet>

// --- Type Definitions ---
using Vector3 = QVector3D; // Use QVector3D for 3D points and vectors.

/**
 * @struct MeshPoint
 * @brief Represents a point in the mesh with a constraint on its connectivity.
 * This structure is key to the constrained reconstruction approach.
 */
//struct MeshPoint {
//    Vector3 pos;                // 3D position of the point.
//    int required_neighbors;     // The exact number of neighbors this point should have in the graph.
//};

//// Represents the connectivity graph. Maps a point index to a set of its neighbor indices.
//using AdjacencyGraph = std::unordered_map<int, std::unordered_set<int>>;

//// Represents a quadrilateral face, defined by the indices of its 4 vertices.
//using QuadFace = std::array<int, 4>;

//// Represents a hexahedral cell, defined by the indices of its 8 vertices.
//using Hexahedron = std::array<int, 8>;


// --- Helper Functions ---

/**
 * @brief Checks if four points are coplanar within a given tolerance.
 * @param points The global list of mesh points.
 * @param face The four point indices defining the plane to check.
 * @param tolerance The floating point tolerance for the check.
 * @return True if the points are coplanar, false otherwise.
 * Calculates the volume of the tetrahedron formed by the four points. If the volume is close to zero, they are coplanar.
 */
inline bool arePointsCoplanar(const std::vector<MeshPoint>& points, const QuadFace& face, float tolerance = 1e-3f) {
    if (face[0] >= (int)points.size() || face[1] >= (int)points.size() ||
        face[2] >= (int)points.size() || face[3] >= (int)points.size()) return false;
    const Vector3& p0 = points[face[0]].pos;
    const Vector3& p1 = points[face[1]].pos;
    const Vector3& p2 = points[face[2]].pos;
    const Vector3& p3 = points[face[3]].pos;
    Vector3 v1 = p1 - p0;
    Vector3 v2 = p2 - p0;
    Vector3 v3 = p3 - p0;
    float volume = QVector3D::dotProduct(v1, QVector3D::crossProduct(v2, v3));
    return std::abs(volume) < tolerance;
}

/**
 * @brief Checks if a face forms a connected cycle in the adjacency graph.
 * @param face The four point indices of the face.
 * @param adjGraph The pre-computed adjacency graph.
 * @return True if p0-p1, p1-p2, p2-p3, and p3-p0 are all connected edges, false otherwise.
 */
inline bool isConnectedFace(const QuadFace& face, const AdjacencyGraph& adjGraph) {
    for (int i = 0; i < 4; ++i) {
        int p_curr = face[i];
        int p_next = face[(i + 1) % 4];
        if (!adjGraph.count(p_curr) || !adjGraph.at(p_curr).count(p_next)) {
            return false;
        }
    }
    return true;
}


namespace ReconstructionEngine {
    /**
     * @brief Step 1: Build the adjacency graph based on precise neighbor constraints.
     * @param points A vector of MeshPoint, each with its own required neighbor count.
     * @return The constructed adjacency graph.
     * This is the foundational step. For each point, it finds exactly the number of nearest neighbors
     * specified by its `required_neighbors` property. This creates a clean, accurate graph.
     */
    inline AdjacencyGraph buildAdjacencyGraph( std::vector<MeshPoint>& points ) {
        AdjacencyGraph adjGraph;
        if (points.empty()) return adjGraph;

        // Iterate through each point to find its neighbors.
        for (size_t i = 0; i < points.size(); ++i) {
            std::vector<std::pair<float, int>> distances;
            // Calculate distance from the current point to all other points.
            for (size_t j = 0; j < points.size(); ++j) {
                if (i == j) continue;
                distances.push_back({points[i].pos.distanceToPoint(points[j].pos), (int)j});
            }
            // Sort points by distance to find the nearest ones.
            std::sort(distances.begin(), distances.end());

            adjGraph[(int)i] = {};
            // Use the specific neighbor count for this point.
            int k_neighbors = points[i].required_neighbors;
            for (int k = 0; k < k_neighbors && k < (int)distances.size(); ++k) {
                adjGraph[(int)i].insert(distances[k].second);

                points[(int)i].neighbor_indices[k] = (int)distances[k].second;

            }
        }
        return adjGraph;
    }

    inline AdjacencyGraph build_adjacencr_graph_from_vector( std::vector<MeshPoint>& points ){

        AdjacencyGraph adjGraph;

        for( size_t i=0; i<points.size();i++ ){

            std::unordered_set<int> neighbouring_pts;
            for( size_t j=0; j<points[i].neighbor_indices.size(); j++ ){

                if ( points[i].neighbor_indices[j] == -1 || abs(points[i].neighbor_indices[j] ) >= int( points.size()) )continue;
                else {

                    neighbouring_pts.insert( points[i].neighbor_indices[j] );
                }
            }

            adjGraph[i] = neighbouring_pts;
        }

        return adjGraph;
    }

    /**
     * @brief Step 2: Identify all valid quadrilateral faces from the graph.
     * @param points The global list of mesh points.
     * @param adjGraph The pre-computed adjacency graph from Step 1.
     * @return A vector of QuadFace representing all valid structural faces.
     * This function searches for 4-cycles in the graph and applies geometric heuristics to filter out non-structural faces.
     */
    inline std::vector<QuadFace> findValidFaces(const std::vector<MeshPoint>& points, const AdjacencyGraph& adjGraph) {
        std::vector<QuadFace> validFaces;
        QSet<QVector<int>> uniqueFaces; // Used to prevent duplicate faces.

        // Iterate through each point as a potential starting corner of a face.
        for (int p0_idx = 0; p0_idx < (int)points.size(); ++p0_idx) {
            if (!adjGraph.count(p0_idx)) continue;

            std::vector<int> neighbors(adjGraph.at(p0_idx).begin(), adjGraph.at(p0_idx).end());

            // Iterate through all pairs of neighbors of p0 to form two sides of a potential quad.
            for (size_t i = 0; i < neighbors.size(); ++i) {
                for (size_t j = i + 1; j < neighbors.size(); ++j) {
                    int p1_idx = neighbors[i];
                    int p3_idx = neighbors[j];

                    if (!adjGraph.count(p1_idx) || !adjGraph.count(p3_idx)) continue;

                    // Search for a fourth point p2 that is a common neighbor of p1 and p3.
                    for (int p2_idx : adjGraph.at(p1_idx)) {
                        if (p2_idx != p0_idx && adjGraph.at(p3_idx).count(p2_idx) && !adjGraph.at(p2_idx).count(p0_idx) ) {
                            QuadFace potentialFace = {p0_idx, p1_idx, p2_idx, p3_idx};

                            // Geometric validation 1: Coplanarity.
                            if (arePointsCoplanar(points, potentialFace)) {

                                    QVector<int> sorted;
                                    QVector<int> sortedFace = {p0_idx, p1_idx, p2_idx, p3_idx};
                                    sorted = sortedFace;
                                    std::sort(sorted.begin(), sorted.end());

                                    // Add the face if it's unique.
                                    if (!uniqueFaces.contains(sorted)) {
                                        validFaces.push_back(potentialFace);
                                        uniqueFaces.insert(sorted);
                                    }

                            }
                        }
                    }
                }
            }
        }
        return validFaces;
    }

    /**
     * @brief Step 3: Build hexahedral cells from the list of valid faces.
     * @param validFaces The list of structural faces from Step 2.
     * @param adjGraph The adjacency graph.
     * @return A vector of unique Hexahedron cells.
     * This function attempts to pair up opposite faces to form hexahedra and then removes duplicates.
     */
    inline std::vector<Hexahedron> buildHexahedra(const std::vector<QuadFace>& validFaces, const AdjacencyGraph& adjGraph) {
            std::vector<Hexahedron> candidateHexahedra;

            // Iterate through all possible pairs of faces to find opposite pairs.
            for (size_t i = 0; i < validFaces.size(); ++i) {
                for (size_t j = i + 1; j < validFaces.size(); ++j) {
                    const auto& face1 = validFaces[i];
                    const auto& face2 = validFaces[j];

                    // --- Check 1: Faces must be disjoint (no shared vertices).
                    QSet<int> face1_pts;
                    for(int p : face1) face1_pts.insert(p);
                    bool disjoint = true;
                    for(int p : face2) {
                        if (face1_pts.contains(p)) {
                            disjoint = false;
                            break;
                        }
                    }
                    if (!disjoint) continue;

                    // --- Check 2: There must be exactly 4 connecting edges between them.
                    std::vector<std::pair<int, int>> connecting_edges;
                    for (int p1 : face1) {
                        if (!adjGraph.count(p1)) continue;
                        for (int p2 : face2) {
                            if (adjGraph.at(p1).count(p2)) {
                                connecting_edges.push_back({p1, p2});
                            }
                        }
                    }

                    if ( connecting_edges.size() == 4 ) {
                        // --- Check 3: Verify that each vertex is used exactly once in the connections.
                        QSet<int> f1_check, f2_check;
                        for( const auto& edge : connecting_edges ) {
                            f1_check.insert(edge.first);
                            f2_check.insert(edge.second);
                        }

                        if ( f1_check.size() == 4 && f2_check.size() == 4 ) {
                            // We found a valid hexahedron candidate.
                            Hexahedron hex;
                            for(int k=0; k<4; ++k) hex[k] = connecting_edges[k].first;
                            for(int k=0; k<4; ++k) hex[k+4] = connecting_edges[k].second;
                            candidateHexahedra.push_back(hex);
                        }
                    }
                }
            }

            // Deduplicate the results.
            std::vector<Hexahedron> finalHexahedra;
            QSet<QVector<int>> uniqueHexes;
            for (const auto& hex : candidateHexahedra) {
                QVector<int> sortedHex(8);
                for(int k=0; k<8; ++k) sortedHex[k] = hex[k];
                std::sort(sortedHex.begin(), sortedHex.end());

                QSet<int> pointSet;
                for(int p_idx : sortedHex) pointSet.insert(p_idx);
                if(pointSet.size() != 8) continue;

                if (!uniqueHexes.contains(sortedHex)) {
                    finalHexahedra.push_back(hex);
                    uniqueHexes.insert(sortedHex);
                }
            }

            return finalHexahedra;
        }

    inline QSet<QVector<float>> MeshSet(std::vector<MeshPoint> points){

        QSet < QVector<float> > return_set;

        for(const auto& point : points){

            if( !return_set.contains( {point.pos.x(), point.pos.y(), point.pos.z()}  )) return_set.insert({point.pos.x(), point.pos.y(), point.pos.z()});
        }

        return  return_set;

    }

} // namespace ReconstructionEngine

#endif // RECONSTRUCTION_ENGINE_H
