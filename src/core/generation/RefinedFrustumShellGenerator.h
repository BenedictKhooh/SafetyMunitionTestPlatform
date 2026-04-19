#ifndef REFINEDFRUSTUMSHELLGENERATOR_H
#define REFINEDFRUSTUMSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class RefinedFrustumShellGenerator : public MeshGenerator {
public:
    RefinedFrustumShellGenerator() = default;

    /**
     * 设置变径圆台壳体的参数
     * @param rBaseIn   底部内半径
     * @param rTopIn    顶部内半径
     * @param wallThickness 壳体壁厚
     * @param hCap      上下端盖厚度
     * @param hVoid     中间空腔高度
     * @param meshSize  全局网格尺寸
     * @param cx,cy,cz  空间偏移坐标
     * @param zStart    局部加密起始高度
     * @param zEnd      局部加密结束高度
     * @param msLocalZ  局部网格尺寸
     */
    void setParameters(double rBaseIn, double rTopIn, double wallThickness,
        double hCap, double hVoid, double meshSize,
        double cx, double cy, double cz,
        double zStart = 0.0, double zEnd = 0.0, double msLocalZ = 0.0) {
        m_rBaseIn = rBaseIn;
        m_rTopIn = rTopIn;
        m_wallThickness = wallThickness;
        m_hCap = hCap;
        m_hVoid = hVoid;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
        m_zStart = zStart;
        m_zEnd = zEnd;
        m_msLocalZ = msLocalZ;
    }

    QString generateGeoScript() const override;
    QString getTypeName() const override { return "RefinedFrustumShell"; }

private:
    double m_rBaseIn = 0.0, m_rTopIn = 0.0;
    double m_wallThickness = 0.0, m_hCap = 0.0, m_hVoid = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
    double m_zStart = 0.0, m_zEnd = 0.0, m_msLocalZ = 0.0;
};

#endif // REFINEDFRUSTUMSHELLGENERATOR_H
