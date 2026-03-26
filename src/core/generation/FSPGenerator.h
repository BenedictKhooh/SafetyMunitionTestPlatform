#ifndef FSPGENERATOR_H
#define FSPGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class FSPGenerator : public MeshGenerator {
public:
    FSPGenerator() = default;

    void setParameters(double radius, double hBody, double hNose, double rTip, double meshSize,
        double cx = 0.0, double cy = 0.0, double cz = 0.0) {
        m_radius = radius;
        m_hBody = hBody;
        m_hNose = hNose;
        m_rTip = rTip;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_radius, m_hBody, m_hNose, m_rTip, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override {
        return "FSP_Fragment";
    }

private:
    QString buildGeoScript(double r, double hb, double hn, double rt, double ms,
        double cx, double cy, double cz) const;

    double m_radius = 5.0;
    double m_hBody = 10.0;
    double m_hNose = 4.0;
    double m_rTip = 2.0;
    double m_meshSize = 0.6;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif // FSPGENERATOR_H