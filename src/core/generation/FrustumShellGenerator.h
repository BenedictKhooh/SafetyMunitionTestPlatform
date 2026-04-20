#ifndef FRUSTUMSHELLGENERATOR_H
#define FRUSTUMSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class FrustumShellGenerator : public MeshGenerator {
public:
    FrustumShellGenerator() = default;

    void setParameters(double rInBot, double rInTop, double wall, double hCap, double hVoid,
        double meshSize, double cx = 0.0, double cy = 0.0, double cz = 0.0, double msWall = 0.0) {
        m_rInBot = rInBot; m_rInTop = rInTop; m_wall = wall;
        m_hCap = hCap; m_hVoid = hVoid;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_msWall = (msWall > 1e-5) ? msWall : meshSize; // 新增壁厚细化支持
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_rInBot, m_rInTop, m_wall, m_hCap, m_hVoid, m_meshSize, m_cx, m_cy, m_cz, m_msWall);
    }

    QString getTypeName() const override { return "FrustumShell"; }

private:
    QString buildGeoScript(double rInBot, double rInTop, double wall, double hCap, double hVoid,
        double ms, double cx, double cy, double cz, double msWall) const;

    double m_rInBot = 0.0, m_rInTop = 0.0, m_wall = 0.0, m_hCap = 0.0, m_hVoid = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_msWall = 0.0;
};

#endif // FRUSTUMSHELLGENERATOR_H