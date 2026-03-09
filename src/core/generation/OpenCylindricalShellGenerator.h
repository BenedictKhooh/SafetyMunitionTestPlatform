#ifndef OPENCYLINDRICALSHELLGENERATOR_H
#define OPENCYLINDRICALSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>
#include <cmath>

class OpenCylindricalShellGenerator : public MeshGenerator {
public:
    OpenCylindricalShellGenerator() = default;

    /**
     * @param cx, cy, cz: 底面中心点坐标
     */
    void setParameters(double radius, double wall, double h_base, double h_wall, double meshSize,
        double cx = 0.0, double cy = 0.0, double cz = 0.0) {
        m_radius = radius;
        m_wall = wall;
        m_hBase = h_base;
        m_hWall = h_wall;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz; // 保存起始点坐标 
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_radius, m_wall, m_hBase, m_hWall, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "OpenCylindricalShell"; }

private:
    QString buildGeoScript(double radius, double wall, double h_base, double h_wall,
        double meshSize, double cx, double cy, double cz) const;

    double m_radius = 0.0, m_wall = 0.0, m_hBase = 0.0, m_hWall = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif