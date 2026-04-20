#ifndef REFINEDHALFCYLINDRICALSHELLGENERATOR_H
#define REFINEDHALFCYLINDRICALSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedHalfCylindricalShellGenerator : public MeshGenerator {
public:
    RefinedHalfCylindricalShellGenerator() = default;

    void setParameters(double rIn, double height, double lid, double wall, double meshSize,
        double cx, double cy, double cz,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0, double msWall = 0.0) {
        m_rIn = rIn;
        m_height = height;
        m_lid = lid;
        m_wall = wall;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart;
        m_zEnd = zEnd;
        m_msLocalZ = msLocalZ;
        m_msWall = (msWall > 1e-5) ? msWall : meshSize; // ÐÂÔö£º±ÚºñÍø¸ñ³ß´ç
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedHalfCylindricalShell"; }

private:
    double m_rIn = 0.0, m_height = 0.0, m_lid = 0.0, m_wall = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0, m_msWall = 0.0;
};

#endif // REFINEDHALFCYLINDRICALSHELLGENERATOR_H