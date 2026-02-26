#ifndef SPHEREGENERATOR_H
#define SPHEREGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class SphereGenerator : public MeshGenerator {
public:
    SphereGenerator() = default;

    // 设置参数：半径、网格大小、以及球心坐标 (cx, cy, cz)
    void setParameters(double radius, double meshSize, double cx, double cy, double cz) {
        m_radius = radius;
        m_meshSize = meshSize;
        m_cx = cx;
        m_cy = cy;
        m_cz = cz;
    }

    // 实现基类接口，供 MeshManager 调用
    QString generateGeoScript() const override {
        return buildGeoScript(m_radius, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "Sphere"; }

private:
    // 修改原有的脚本构建逻辑，加入偏移量
    QString buildGeoScript(double radius, double meshSize, double cx, double cy, double cz) const;

    double m_radius = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif