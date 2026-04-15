#ifndef FRUSTUMGENERATOR_H
#define FRUSTUMGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class FrustumGenerator : public MeshGenerator {
public:
    FrustumGenerator() = default;

    /**
     * 设置参数
     * @param rBase: 底部半径
     * @param rTop: 顶部半径（若为0则退化为圆锥，但建议保留极小值以维持O-Grid结构）
     * @param height: 高度
     * @param meshSize: 目标网格大小
     * @param cx, cy, cz: 底面圆心坐标
     */
    void setParameters(double rBase, double rTop, double height, double meshSize, double cx, double cy, double cz) {
        m_rBase = rBase;
        m_rTop = rTop;
        m_height = height;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_rBase, m_rTop, m_height, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "Frustum"; }

private:
    QString buildGeoScript(double rBase, double rTop, double height, double meshSize, double cx, double cy, double cz) const;

    double m_rBase = 0.0, m_rTop = 0.0, m_height = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif // FRUSTUMGENERATOR_H