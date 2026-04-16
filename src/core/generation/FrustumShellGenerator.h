#ifndef FRUSTUMSHELLGENERATOR_H
#define FRUSTUMSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class FrustumShellGenerator : public MeshGenerator {
public:
    FrustumShellGenerator() = default;

    // 简化后的接口：几何尺寸 + 唯一网格尺寸 + 坐标
    void setParameters(double rInBot, double rInTop, double wall, double hCap, double hVoid,
        double meshSize, double cx = 0.0, double cy = 0.0, double cz = 0.0) {
        m_rInBot = rInBot; m_rInTop = rInTop; m_wall = wall;
        m_hCap = hCap; m_hVoid = hVoid;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_rInBot, m_rInTop, m_wall, m_hCap, m_hVoid, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "FrustumShell"; }

private:
    QString buildGeoScript(double rInBot, double rInTop, double wall, double hCap, double hVoid,
        double ms, double cx, double cy, double cz) const;

    double m_rInBot, m_rInTop, m_wall, m_hCap, m_hVoid, m_meshSize;
    double m_cx, m_cy, m_cz;
};

#endif