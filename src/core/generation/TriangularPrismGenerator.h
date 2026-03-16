#ifndef TRIANGULARPRISMGENERATOR_H
#define TRIANGULARPRISMGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class TriangularPrismGenerator : public MeshGenerator {
public:
    TriangularPrismGenerator() = default;

    /**
     * 设置参数
     * @param radius     外接圆半径
     * @param height     三棱柱高度
     * @param meshSize   网格目标尺寸
     * @param cx, cy, cz 中心点偏移
     */
    void setParameters(double radius, double height, double meshSize, double cx, double cy, double cz) {
        m_radius = radius;
        m_height = height;
        m_meshSize = meshSize;
        m_cx = cx;
        m_cy = cy;
        m_cz = cz;
    }

    // 实现基类接口
    QString generateGeoScript() const override {
        return buildGeoScript(m_radius, m_height, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "TriangularPrism"; }

private:
    QString buildGeoScript(double radius, double height, double meshSize, double cx, double cy, double cz) const;

    double m_radius = 0.0;
    double m_height = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif // TRIANGULARPRISMGENERATOR_H