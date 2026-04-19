#ifndef REFINEDHALFCYLINDRICALSHELLGENERATOR_H
#define REFINEDHALFCYLINDRICALSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedHalfCylindricalShellGenerator : public MeshGenerator {
public:
    RefinedHalfCylindricalShellGenerator() = default;

    /**
     * 设置带局部加密的半圆柱壳参数
     * @param rIn        内腔半径
     * @param height     总高度
     * @param lid        上下端盖厚度
     * @param wall       壳体壁厚
     * @param meshSize   全局网格尺寸
     * @param cx,cy,cz   空间中心偏移坐标
     * @param zStart     局部加密起始高度
     * @param zEnd       局部加密结束高度
     * @param msLocalZ   局部加密区间网格尺寸
     */
    void setParameters(double rIn, double height, double lid, double wall, double meshSize,
        double cx, double cy, double cz,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0) {
        m_rIn = rIn;
        m_height = height;
        m_lid = lid;
        m_wall = wall;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart;
        m_zEnd = zEnd;
        m_msLocalZ = msLocalZ;
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedHalfCylindricalShell"; }

private:
    double m_rIn = 0.0, m_height = 0.0, m_lid = 0.0, m_wall = 0.0, m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0;
};

#endif // REFINEDHALFCYLINDRICALSHELLGENERATOR_H