#ifndef REFINEDFRUSTUMGENERATOR_H
#define REFINEDFRUSTUMGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedFrustumGenerator : public MeshGenerator {
public:
    RefinedFrustumGenerator() = default;

    void setParameters(double rBase, double rTop, double height, double meshSize, double cx, double cy, double cz,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0) {
        m_rBase = rBase; m_rTop = rTop;
        m_height = height; m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart; m_zEnd = zEnd; m_msLocalZ = msLocalZ;
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedFrustum"; }

private:
    double m_rBase = 0.0, m_rTop = 0.0, m_height = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0;
};

#endif // REFINEDFRUSTUMGENERATOR_H
