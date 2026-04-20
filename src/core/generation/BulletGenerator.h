#ifndef BULLETGENERATOR_H
#define BULLETGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class BulletGenerator : public MeshGenerator {
public:
    BulletGenerator() = default;

    // 设置弹头参数：口径、被甲厚度、圆柱长、弹头长、网格基准尺寸、以及空间偏移坐标
    // (增加了更精细的比例和网格控制参数，带有默认值以保证兼容性)
    void setParameters(double caliber, double jacketThickness,
        double cylinderLength, double noseLength,
        double meshSize,
        double cx = 0.0, double cy = 0.0, double cz = 0.0,
        double tipDiameter = 1.0, double coreRatio = 0.6,
        double progNose = 0.9)
    {
        m_caliber = caliber;
        m_jacketThickness = jacketThickness;
        m_cylinderLength = cylinderLength;
        m_noseLength = noseLength;

        m_meshSizeXY = meshSize;
        m_meshSizeZCyl = meshSize * 1.5;  // 圆柱段Z向网格可以适当放宽以减少计算量
        m_meshSizeZNose = meshSize * 1.2; // 弹头段Z向基础网格

        m_cx = cx;
        m_cy = cy;
        m_cz = cz;

        m_tipDiameter = tipDiameter;
        m_coreRatio = coreRatio;
        m_progNose = progNose;
    }

    // 实现基类接口，供 MeshManager 调用
    QString generateGeoScript() const override {
        return buildGeoScript();
    }

    QString getTypeName() const override { return "Bullet"; }

private:
    QString buildGeoScript() const;

    // 几何尺寸参数
    double m_caliber = 7.62;
    double m_jacketThickness = 0.6;
    double m_cylinderLength = 12.0;
    double m_noseLength = 16.0;
    double m_tipDiameter = 1.0;
    double m_coreRatio = 0.6;

    // 网格控制参数
    double m_meshSizeXY = 0.5;
    double m_meshSizeZCyl = 0.8;
    double m_meshSizeZNose = 0.6;
    double m_progNose = 0.9; // 尖端渐变系数

    // 空间位置偏移
    double m_cx = 0.0;
    double m_cy = 0.0;
    double m_cz = 0.0;
};

#endif // BULLETGENERATOR_H