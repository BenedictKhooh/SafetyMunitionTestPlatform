#ifndef HALFCYLINDERGENERATOR_H
#define HALFCYLINDERGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

/**
 * @class HalfCylinderGenerator
 * @brief 实现半圆柱体结构化 O-Grid 网格生成器
 */
class HalfCylinderGenerator : public MeshGenerator {
public:
    HalfCylinderGenerator() = default;

    /**
     * @brief 设置参数
     * @param radius 半径
     * @param height 高度
     * @param meshSize 期望的网格特征尺寸（用于自动计算分段数）
     * @param cx, cy, cz 中心底部的偏移坐标
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

    QString getTypeName() const override { return "HalfCylinder"; }

private:
    QString buildGeoScript(double radius, double height, double meshSize, double cx, double cy, double cz) const;

    double m_radius = 0.0;
    double m_height = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif // HALFCYLINDERGENERATOR_H