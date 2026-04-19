#ifndef REFINEDCYLINDRICALSHELLGENERATOR_H
#define REFINEDCYLINDRICALSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedCylindricalShellGenerator : public MeshGenerator {
public:
    RefinedCylindricalShellGenerator() = default;

    void setParameters(double rIn, double wallThickness, double hCap, double hVoid, double meshSize,
        double cx, double cy, double cz,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0) {
        m_rIn = rIn;
        m_wallThickness = wallThickness;
        m_hCap = hCap;
        m_hVoid = hVoid;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart;
        m_zEnd = zEnd;
        m_msLocalZ = msLocalZ;
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedCylindricalShell"; }

private:
    double m_rIn = 0.0, m_wallThickness = 0.0, m_hCap = 0.0, m_hVoid = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0;
};

#endif // REFINEDCYLINDRICALSHELLGENERATOR_H
