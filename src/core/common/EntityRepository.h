#pragma once
#ifndef ENTITYREPOSITORY_H
#define ENTITYREPOSITORY_H

#include <QString>
#include <map>
#include <vector>
// 包含你项目里定义这些基础结构体的头文件
#include "core/common/common_types.h" // 假设 MeshPoint, Hexahedron, Vector3 在这里


struct Boundary {
    std::string name;               // 边界的名称 (例如 "Top_Surface", "Impact_Face")
    std::vector<size_t> nodeIndices; // 储存属于该边界的节点在 MeshEntity.nodes 中的索引
};
// --- 1. 单个实体的属性结构 ---
struct MeshEntity {
    QString name;                       // 实体名称 (如 "ball1")
    QString type;                       // 实体类型 (如 "Sphere", "Cylinder")
    std::vector<MeshPoint> nodes;       // 节点数据
    std::vector<Hexahedron> hexes;      // 六面体单元数据
    std::vector<Vector3> wireLines;     // 预生成的渲染线段 (缓存起来，重绘时极快)

    std::vector<Boundary> boundaries; // [新增] 用于储存该实体内部包含的所有边界
};

// --- 2. 实体仓库管理类 ---
class EntityRepository {
public:
    EntityRepository() = default;

    // 增：添加一个实体
    bool addEntity(const QString& name, const MeshEntity& entity);

    // 删：根据名字删除实体
    bool deleteEntity(const QString& name);

    // 查：判断是否存在
    bool hasEntity(const QString& name) const;

    // 查：获取特定实体（只读）
    const MeshEntity* getEntity(const QString& name) const;

    // 查：获取所有实体的引用（用于遍历渲染）
    const std::map<QString, MeshEntity>& getAllEntities() const;

    // 清空整个仓库
    void clearAll();

private:
    std::map<QString, MeshEntity> m_entities; // 核心字典，用名字做索引
};

#endif // ENTITYREPOSITORY_H