#ifndef CUBEGENERATOR_H
#define CUBEGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class CubeGenerator : public MeshGenerator {
public:
    CubeGenerator() = default;

    /**
     * 设置参数
     * @param lx, ly, lz: 立方体在 X, Y, Z 方向的长度
     * @param meshSize: 网格大小
     * @param cx, cy, cz: 中心点坐标
     */
    void setParameters(double lx, double ly, double lz, double meshSize, double cx, double cy, double cz) {
        m_lx = lx; m_ly = ly; m_lz = lz;
        m_meshSize = meshSize;
        m_cx = cx; m_cy = cy; m_cz = cz;
    }

    QString generateGeoScript() const override {
        return buildGeoScript(m_lx, m_ly, m_lz, m_meshSize, m_cx, m_cy, m_cz);
    }

    QString getTypeName() const override { return "Box"; }

private:
    QString buildGeoScript(double lx, double ly, double lz, double meshSize, double cx, double cy, double cz) const;

    double m_lx = 0.0, m_ly = 0.0, m_lz = 0.0;
    double m_meshSize = 0.0;
    double m_cx = 0.0, m_cy = 0.0, m_cz = 0.0;
};

#endif