#ifndef BULLETGENERATOR_H
#define BULLETGENERATOR_H

#include "src/core/common/MeshGenerator.h"
#include <QString>

class BulletGenerator : public MeshGenerator {
public:
    BulletGenerator() = default;

    /**
     * @brief 设置子弹网格参数 (单位统一为 cm)
     * @param caliber        口径 (默认 0.762 cm)
     * @param jacketThickness 被甲厚度 (默认 0.06 cm)
     * @param cylinderLength 圆柱段长度 (默认 1.2 cm)
     * @param noseLength     弹头段长度 (默认 1.6 cm)
     * @param meshSize       基础网格尺寸 (默认 0.05 cm)
     * @param cx             空间中心点 X
     * @param cy             空间中心点 Y
     * @param cz             空间中心点 Z (底部基准面)
     * @param tipDiameter    尖端平切直径 (防止畸变，默认 0.1 cm)
     * @param progNose       Z轴渐变系数 (越靠近尖端网格越密，默认 0.9)
     */
    void setParameters(double caliber, double jacketThickness,
        double cylinderLength, double noseLength,
        double meshSize,
        double cx = 0.0, double cy = 0.0, double cz = 0.0,
        double tipDiameter = 0.1, double progNose = 0.9)
    {
        m_caliber = caliber;
        m_jacketThickness = jacketThickness;
        m_cylinderLength = cylinderLength;
        m_noseLength = noseLength;

        m_meshSizeXY = meshSize;
        m_meshSizeZCyl = meshSize * 1.6;  // 圆柱段Z向网格适当拉长，提高计算效率
        m_meshSizeZNose = meshSize * 1.2; // 弹头段Z向基础网格

        m_cx = cx;
        m_cy = cy;
        m_cz = cz;

        m_tipDiameter = tipDiameter;
        m_progNose = progNose;
    }

    // 实现基类接口，供 MeshManager 调用
    QString generateGeoScript() const override {
        return buildGeoScript();
    }

    QString getTypeName() const override { return "Bullet"; }

private:
    QString buildGeoScript() const;

    // 宏观物理尺寸 (cm)
    double m_caliber = 0.762;
    double m_jacketThickness = 0.06;
    double m_cylinderLength = 1.2;
    double m_noseLength = 1.6;
    double m_tipDiameter = 0.1;

    // 网格与拓扑控制
    double m_meshSizeXY = 0.05;
    double m_meshSizeZCyl = 0.08;
    double m_meshSizeZNose = 0.06;
    double m_progNose = 0.9;

    // 空间位置偏移
    double m_cx = 0.0;
    double m_cy = 0.0;
    double m_cz = 0.0;
};

#endif // BULLETGENERATOR_H