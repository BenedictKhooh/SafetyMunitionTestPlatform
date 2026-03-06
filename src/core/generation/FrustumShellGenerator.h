#ifndef FRUSTUMSHELLGENERATOR_H
#define FRUSTUMSHELLGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

/**
 * @class FrustumShellGenerator
 * @brief 实现变径圆台容器（带端盖、空腔、结构化O-Grid网格）的生成器
 */
class FrustumShellGenerator : public MeshGenerator {
public:
    FrustumShellGenerator() = default;

    /**
     * @brief 设置几何与网格参数
     * @param rInBot   底部内半径
     * @param rInTop   顶部内半径
     * @param wall     壳体壁厚
     * @param hCap     端盖厚度
     * @param hVoid    中间空腔高度
     * @param nC       圆弧向网格数
     * @param nRIn     内径径向网格数
     * @param nRWall   壁厚径向网格数
     * @param nHCap    端盖垂直层数
     * @param nHVoid   空腔段垂直层数
     */
    void setParameters(double rInBot, double rInTop, double wall, double hCap, double hVoid,
        int nC, int nRIn, int nRWall, int nHCap, int nHVoid) {
        m_rInBot = rInBot;
        m_rInTop = rInTop;
        m_wall = wall;
        m_hCap = hCap;
        m_hVoid = hVoid;
        m_nC = nC;
        m_nRIn = nRIn;
        m_nRWall = nRWall;
        m_nHCap = nHCap;
        m_nHVoid = nHVoid;
    }

    // 实现基类接口
    QString generateGeoScript() const override {
        return buildGeoScript(m_rInBot, m_rInTop, m_wall, m_hCap, m_hVoid,
            m_nC, m_nRIn, m_nRWall, m_nHCap, m_nHVoid);
    }

    QString getTypeName() const override { return "FrustumShell"; }

private:
    QString buildGeoScript(double rInBot, double rInTop, double wall, double hCap, double hVoid,
        int nC, int nRIn, int nRWall, int nHCap, int nHVoid) const;

    // 几何参数
    double m_rInBot = 0.0, m_rInTop = 0.0, m_wall = 0.0, m_hCap = 0.0, m_hVoid = 0.0;
    // 网格控制参数
    int m_nC = 10, m_nRIn = 8, m_nRWall = 5, m_nHCap = 6, m_nHVoid = 25;
};

#endif // FRUSTUMSHELLGENERATOR_H