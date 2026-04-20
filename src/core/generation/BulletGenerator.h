#ifndef BULLETGENERATOR_H
#define BULLETGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class BulletGenerator : public MeshGenerator {
public:
    BulletGenerator() = default;

    // 移除了 progNose 参数，直接在内部定死
    void setParameters(double caliber, double jacketThickness,
        double cylinderLength, double noseLength,
        double meshSize,
        double cx = 0.0, double cy = 0.0, double cz = 0.0,
        double tipDiameter = 0.1)
    {
        m_caliber = caliber;
        m_jacketThickness = jacketThickness;
        m_cylinderLength = cylinderLength;
        m_noseLength = noseLength;

        m_meshSizeXY = meshSize;
        m_meshSizeZCyl = meshSize * 1.6;
        m_meshSizeZNose = meshSize * 1.2;

        m_cx = cx;
        m_cy = cy;
        m_cz = cz;

        m_tipDiameter = tipDiameter;
    }

    QString generateGeoScript() const override {
        return buildGeoScript();
    }

    QString getTypeName() const override { return "Bullet"; }

private:
    QString buildGeoScript() const;

    double m_caliber = 0.762;
    double m_jacketThickness = 0.06;
    double m_cylinderLength = 1.2;
    double m_noseLength = 1.6;
    double m_tipDiameter = 0.1;

    double m_meshSizeXY = 0.05;
    double m_meshSizeZCyl = 0.08;
    double m_meshSizeZNose = 0.06;

    double m_cx = 0.0;
    double m_cy = 0.0;
    double m_cz = 0.0;
};

#endif // BULLETGENERATOR_H