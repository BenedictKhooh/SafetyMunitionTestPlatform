#ifndef HALFCYLINDRICALSHELLGENERATOR_H
#define HALFCYLINDRICALSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class HalfCylindricalShellGenerator : public MeshGenerator {
public:
    HalfCylindricalShellGenerator() = default;

    /**
     * @brief 设置参数
     * @param radius 内径 (R_in)
     * @param height 总高度
     * @param lid 端盖厚度 (H_cap)
     * @param wall 壁厚 (Wall)
     * @param meshSize 单元特征尺寸
     * @param cx, cy, cz 中心底部的偏移坐标
     */
    void setParameters(double radius, double height, double lid, double wall, double meshSize,
        double cx, double cy, double cz) {
        m_radius = radius;
        m_height = height;
        m_lid = lid;
        m_wall = wall;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_radius, m_height, m_lid, m_wall, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "HalfCylindricalShell"; }

private:
    QString buildGeoScript(double radius, double height, double lid, double wall, double meshSize,
        double cx, double cy, double cz) const;

    double m_radius = 0.0, m_height = 0.0, m_lid = 0.0, m_wall = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif
