#pragma once

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <vector>
#include <array>
#include <QtMath>
#include <unordered_map>
#include <unordered_set>
#include <QSet>
#include <QVector3D>
#include <QVector2D>

// --- Common Type Definitions ---
using Vector3 = QVector3D;
using Vector2 = QVector2D;

/**
 * @struct MeshPoint
 * @brief Represents a point in the mesh with its position and connectivity constraint.
 * This is the fundamental data structure for all points in the project.
 */
struct MeshPoint {
    Vector3 pos;

    int required_neighbors;

    std::vector<int> neighbor_indices; // Stores indices of neighboring points in the final mesh array.
	std::vector<MeshPoint> neighbor_points;

};

struct MeshPoint_gmsh {

    int instance_id;
    Vector3 pos;
};

struct SamplingMeshPoint {

    Vector3 pos;
    QSet<SamplingMeshPoint*> neighbor_meshpoints;

    explicit SamplingMeshPoint( const Vector3& initial_pos )
        : pos(initial_pos){

    }

};

struct SamplingSpline {

    std::vector<SamplingMeshPoint*> meshpoint_list;
    SamplingSpline* next;
    SamplingSpline* prev;
};

struct SamplingPlane {

    SamplingSpline* spline_starter;
    SamplingPlane* next;
    SamplingPlane* prev;
};

struct Vector3Comparator {
    bool operator()(const Vector3& a, const Vector3& b) const {
        const float epsilon = 1e-6f;
        if (std::abs(a.x() - b.x()) > epsilon) return a.x() < b.x();
        if (std::abs(a.y() - b.y()) > epsilon) return a.y() < b.y();
        if (std::abs(a.z() - b.z()) > epsilon) return a.z() < b.z();
        return false; // Considered equal
    }
};

// 用于 std::map<MeshPoint, ...>
struct MeshPointComparator {
    bool operator()(const MeshPoint& a, const MeshPoint& b) const {
        // 比较逻辑封装在这里，而不是在 MeshPoint 内部
        const float epsilon = 1e-6f;
        if (std::abs(a.pos.x() - b.pos.x()) > epsilon) return a.pos.x() < b.pos.x();
        if (std::abs(a.pos.y() - b.pos.y()) > epsilon) return a.pos.y() < b.pos.y();
        if (std::abs(a.pos.z() - b.pos.z()) > epsilon) return a.pos.z() < b.pos.z();
        return false; // Considered equal
    }
};

struct MeshPointComparator_gmsh {
    bool operator()(const MeshPoint_gmsh& a, const MeshPoint_gmsh& b) const {
        // 比较逻辑封装在这里，而不是在 MeshPoint 内部
        const float epsilon = 1e-6f;
        if (std::abs(a.pos.x() - b.pos.x()) > epsilon) return a.pos.x() < b.pos.x();
        if (std::abs(a.pos.y() - b.pos.y()) > epsilon) return a.pos.y() < b.pos.y();
        if (std::abs(a.pos.z() - b.pos.z()) > epsilon) return a.pos.z() < b.pos.z();
        return false; // Considered equal
    }
};


// 4. 为 std::unordered_map 提供自定义哈希函数和等价函数
struct MeshPointHasher {
    std::size_t operator()(const MeshPoint& p) const {
        // 哈希逻辑封装在这里
        std::size_t hx = std::hash<float>{}(p.pos.x());
        std::size_t hy = std::hash<float>{}(p.pos.y());
        std::size_t hz = std::hash<float>{}(p.pos.z());
        return hx ^ (hy << 1) ^ (hz << 2);
    }
};

struct MeshPointHasher_gmsh {
    std::size_t operator()(const MeshPoint_gmsh& p) const {
        // 哈希逻辑封装在这里
        std::size_t hx = std::hash<float>{}(p.pos.x());
        std::size_t hy = std::hash<float>{}(p.pos.y());
        std::size_t hz = std::hash<float>{}(p.pos.z());
        return hx ^ (hy << 1) ^ (hz << 2);
    }
};

struct MeshPointEqualTo {
    bool operator()(const MeshPoint& a, const MeshPoint& b) const {
        // 等价逻辑封装在这里
        const float epsilon = 1e-6f;
        return std::abs(a.pos.x() - b.pos.x()) < epsilon &&
               std::abs(a.pos.y() - b.pos.y()) < epsilon &&
               std::abs(a.pos.z() - b.pos.z()) < epsilon &&
               a.required_neighbors == b.required_neighbors;
    }
};

struct MeshPointEqualTo_gmsh {
    bool operator()(const MeshPoint_gmsh& a, const MeshPoint_gmsh& b) const {
        // 等价逻辑封装在这里
        const float epsilon = 1e-6f;
        return std::abs(a.pos.x() - b.pos.x()) < epsilon &&
            std::abs(a.pos.y() - b.pos.y()) < epsilon &&
            std::abs(a.pos.z() - b.pos.z()) < epsilon &&
            std::abs(a.instance_id - b.instance_id) < epsilon;
            
    }
};

// Represents the connectivity graph. Maps a point index to a set of its neighbor indices.
using AdjacencyGraph = std::unordered_map<int, std::unordered_set<int>>;

// Represents a quadrilateral face, defined by the indices of its 4 vertices.
using QuadFace = std::array<int, 4>;

// Represents a hexahedral cell, defined by the indices of its 8 vertices.
using Hexahedron = std::array<int, 8>;

// Define an instance
struct Instance {

    QString instance_name;

    std::vector< MeshPoint > instance_points;
    AdjacencyGraph instance_adjacenctgraph;
    std::vector< QuadFace > instance_quadface_list;
    std::vector< Hexahedron > instance_hexahedron_list;

	explicit Instance(const QString& name, const std::vector< MeshPoint > & points)
		: instance_name(name), instance_points(points){
	}

    //std::vector<Boundaries> instance_boundaries;

    //Instance() = default;
};

struct Instance_gmsh {

    QString instance_name;

    std::vector< MeshPoint_gmsh > instance_points;
};

struct eos{

    QString eos_name;
    std::vector< double > eos_params;
};

struct Triangle{

    Vector3 normal;      // 12 bytes
    Vector3 vertices[3]; // 3 * 12 = 36 bytes
    uint16_t attribute;  // 2 bytes

};//for building triangle facet

struct GeoLine {

    int resolution = 100;
    std::vector<Vector3> points;

    explicit GeoLine(Vector3& start, Vector3& end, const int resolu) {
    
        resolution = resolu;
        for (int i = 0; i < resolu; i++) {

            float x_section = start.x() + i * (end.x() - start.x()) / resolu;
            float y_section = start.y() + i * (end.y() - start.y()) / resolu;
            float z_section = start.z() + i * (end.z() - start.z()) / resolu;
            points.push_back(Vector3(x_section, y_section, z_section));
        }
    }
    explicit GeoLine(Vector3& start, Vector3& end) {

        for (int i = 0; i < resolution; i++) {

            float x_section = start.x() + i * (end.x() - start.x()) / resolution;
            float y_section = start.y() + i * (end.y() - start.y()) / resolution;
            float z_section = start.z() + i * (end.z() - start.z()) / resolution;
            points.push_back(Vector3(x_section, y_section, z_section));
        }
    }

    explicit GeoLine(MeshPoint& start, MeshPoint& end, int resolu ){

        resolution = resolu;
        for(int i=0; i<resolu; i++){

            float x_section = start.pos.x() + i*(end.pos.x() - start.pos.x() )/resolu;
            float y_section = start.pos.y() + i*(end.pos.y() - start.pos.y() )/resolu;
            float z_section = start.pos.z() + i*(end.pos.z() - start.pos.z() )/resolu;
            points.push_back(Vector3(x_section, y_section, z_section));
        }

    }

    explicit GeoLine(MeshPoint& start, MeshPoint& end) {

        for (int i = 0; i < resolution; i++) {

            float x_section = start.pos.x() + i * (end.pos.x() - start.pos.x()) / resolution;
            float y_section = start.pos.y() + i * (end.pos.y() - start.pos.y()) / resolution;
            float z_section = start.pos.z() + i * (end.pos.z() - start.pos.z()) / resolution;
            points.push_back(Vector3(x_section, y_section, z_section));
        }

    }
};

struct Boundaries {

    std::vector<MeshPoint> boundary_points;
};

struct HexElement {
    int id;
    int nodeIndices[8]; // 存储 8 个节点的编号
};

struct Element {

    std::vector< MeshPoint_gmsh* > element_points;
};

#endif // COMMON_TYPES_H
