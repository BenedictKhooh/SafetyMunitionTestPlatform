#ifndef REFINEDFRUSTUMSHELLGENERATOR_H
#define REFINEDFRUSTUMSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedFrustumShellGenerator : public MeshGenerator {
public:
    RefinedFrustumShellGenerator() = default;

    void setParameters(double rBaseIn, double rTopIn, double wallThickness,
        double hCap, double hVoid, double meshSize,
        double cx, double cy, double cz,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0, double msWall = 0.0) {
        m_rBaseIn = rBaseIn;
        m_rTopIn = rTopIn;
        m_wallThickness = wallThickness;
        m_hCap = hCap;
        m_hVoid = hVoid;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart;
        m_zEnd = zEnd;
        m_msLocalZ = msLocalZ;
        m_msWall = (msWall > 1e-5) ? msWall : meshSize;
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedFrustumShell"; }

private:
    double m_rBaseIn = 0.0, m_rTopIn = 0.0;
    double m_wallThickness = 0.0, m_hCap = 0.0, m_hVoid = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0, m_msWall = 0.0;
};

#endif // REFINEDFRUSTUMSHELLGENERATOR_H