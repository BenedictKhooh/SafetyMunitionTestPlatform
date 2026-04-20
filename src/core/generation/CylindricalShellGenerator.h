#ifndef CYLINDRICALSHELLGENERATOR_H
#define CYLINDRICALSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>
#include <cmath>

class CylindricalShellGenerator : public MeshGenerator {
public:
    CylindricalShellGenerator() = default;

    /**
     * 设置参数
     * @param radius: 内径
     * @param height: 总高度
     * @param lid: 上下端盖厚度
     * @param wall: 侧壁厚度
     * @param meshSize: 全局网格尺寸
     * @param cx, cy, cz: 初始中心坐标
     * @param msWall: 壁厚方向的局部网格尺寸 (如果为0，则使用 meshSize)
     */
    void setParameters(double radius, double height, double lid, double wall, double meshSize,
        double cx = 0.0, double cy = 0.0, double cz = 0.0, double msWall = 0.0) {
        m_radius = radius;
        m_height = height;
        m_lid = lid;
        m_wall = wall;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        // 如果未指定壁厚细化尺寸，则默认等同于全局网格尺寸
        m_msWall = (msWall > 1e-5) ? msWall : meshSize;
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_radius, m_height, m_lid, m_wall, m_meshSize, m_cx, m_cy, m_cz, m_msWall);
    }

    QString getTypeName() const override { return "CylindricalShell"; }

private:
    QString buildGeoScript(double radius, double height, double lid, double wall,
        double meshSize, double cx, double cy, double cz, double msWall) const;

    double m_radius = 0.0, m_height = 0.0, m_lid = 0.0, m_wall = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_msWall = 0.0; // 新增：壁厚网格尺寸
};

#endif // CYLINDRICALSHELLGENERATOR_H