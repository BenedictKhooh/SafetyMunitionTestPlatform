#ifndef REFINEDCYLINDERGENERATOR_H
#define REFINEDCYLINDERGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedCylinderGenerator : public MeshGenerator {
public:
    RefinedCylinderGenerator() = default;

    void setParameters(double radius, double meshSize, double height, double cx, double cy, double cz,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0) {
        m_radius = radius;
        m_meshSize = meshSize;
        m_height = height;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart;
        m_zEnd = zEnd;
        m_msLocalZ = msLocalZ;
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedCylinder"; }

private:
    double m_radius = 0.0, m_meshSize = 0.0, m_height = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0;
};

#endif // REFINEDCYLINDERGENERATOR_H