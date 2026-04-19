#ifndef REFINEDOPENCYLINDRICALSHELLGENERATOR_H
#define REFINEDOPENCYLINDRICALSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedOpenCylindricalShellGenerator : public MeshGenerator {
public:
    RefinedOpenCylindricalShellGenerator() = default;

    /**
     * 设置带局部加密的开式圆柱壳参数 (杯子形状：有底盘、无顶盖)
     * @param radius     内腔半径
     * @param wall       壳体壁厚
     * @param hBase      底盘厚度
     * @param hWall      侧壁高度
     * @param meshSize   全局网格尺寸
     * @param cx,cy,cz   空间中心偏移坐标
     * @param zStart     局部加密起始高度
     * @param zEnd       局部加密结束高度
     * @param msLocalZ   局部加密区间网格尺寸
     */
    void setParameters(double radius, double wall, double hBase, double hWall, double meshSize,
        double cx = 0.0, double cy = 0.0, double cz = 0.0,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0) {
        m_radius = radius;
        m_wall = wall;
        m_hBase = hBase;
        m_hWall = hWall;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart;
        m_zEnd = zEnd;
        m_msLocalZ = msLocalZ;
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedOpenCylindricalShell"; }

private:
    double m_radius = 0.0, m_wall = 0.0, m_hBase = 0.0, m_hWall = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0;
};

#endif // REFINEDOPENCYLINDRICALSHELLGENERATOR_H