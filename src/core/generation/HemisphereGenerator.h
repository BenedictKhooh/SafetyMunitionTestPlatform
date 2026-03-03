#ifndef HEMISPHEREGENERATOR_H
#define HEMISPHEREGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class HemisphereGenerator : public MeshGenerator {
public:
    HemisphereGenerator() = default;

    /**
     * 设置参数
     * @param radius: 半球半径
     * @param meshSize: 目标网格尺寸
     * @param cx, cy, cz: 半球底面圆心坐标
     */
    void setParameters(double radius, double meshSize, double cx, double cy, double cz) {
        m_radius = radius;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_radius, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "Hemisphere"; }

private:
    QString buildGeoScript(double radius, double meshSize, double cx, double cy, double cz) const;

    double m_radius = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif