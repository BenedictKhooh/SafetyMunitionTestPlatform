#include "HalfCylinderGenerator.h"
#include <QtMath>

/**
 * @brief 构建用于生成完整半圆柱 O-Grid 网格的 Gmsh 脚本
 */
QString HalfCylinderGenerator::buildGeoScript(double radius, double height, double meshSize, double cx, double cy, double cz) const {

    // --- 1. 基于 meshSize 自动计算各方向的网格分段数 ---
    // Rproj 是投影到 45 度位置的坐标值
    double Rproj_val = radius / 1.41421356;
    double L_val = Rproj_val / 2.0;

    // nC: 顶部弧线(90deg)和中心矩形横边的点数
    int nC = qMax(2, (int)qRound(Rproj_val / meshSize));
    // nL: 两侧 45deg 弧线和中心矩形竖边的点数
    int nL = qMax(2, (int)qRound(L_val / meshSize));
    // nR: 径向（从内向外）的点数
    int nR = qMax(2, (int)qRound((radius - L_val) / meshSize));
    // nH: 高度方向的点数
    int nH = qMax(2, (int)qRound(height / meshSize));

    return QString(R"(
////////////////////////////////////////////////////
// Half-Cylindrical O-grid mesh (Full Volume Version)
////////////////////////////////////////////////////

// 1. 参数定义 (由 C++ 注入)
R = %1;
H = %2;
nC = %3;
nL = %4;
nR = %5;
nH = %6;

// 偏移位置
CX = %7; CY = %8; CZ = %9;

// 尺寸预计算
Rproj = R / 1.41421356; 
L = Rproj / 2.0; 

// 2. 点定义 (加入空间偏移)
Point(1) = {CX + 0, CY + 0, CZ + 0}; // 圆心基准

// 中心矩形块顶点
Point(2) = {CX - L, CY + 0, CZ + 0};
Point(3) = {CX + L, CY + 0, CZ + 0};
Point(4) = {CX + L, CY + L, CZ + 0};
Point(5) = {CX - L, CY + L, CZ + 0};

// 外围半圆顶点
Point(6) = {CX - R,     CY + 0,     CZ + 0};
Point(7) = {CX + R,     CY + 0,     CZ + 0};
Point(8) = {CX + Rproj, CY + Rproj, CZ + 0};
Point(9) = {CX - Rproj, CY + Rproj, CZ + 0};

// 3. 线定义
// 内部矩形边框
Line(1) = {2, 3}; 
Line(2) = {3, 4}; 
Line(3) = {4, 5}; 
Line(4) = {5, 2}; 

// 径向连接线
Line(5) = {6, 2}; 
Line(6) = {3, 7}; 
Line(7) = {4, 8}; 
Line(8) = {5, 9}; 

// 外围圆弧
Circle(9)  = {7, 1, 8}; 
Circle(10) = {8, 1, 9}; 
Circle(11) = {9, 1, 6}; 

// 4. 面定义 (定义构成半圆底面的 4 个子面)
Curve Loop(1) = {1, 2, 3, 4};       Plane Surface(1) = {1}; // 中心块
Curve Loop(2) = {6, 9, -7, -2};     Plane Surface(2) = {2}; // 右侧块
Curve Loop(3) = {7, 10, -8, -3};    Plane Surface(3) = {3}; // 顶部块
Curve Loop(4) = {8, 11, 5, -4};     Plane Surface(4) = {4}; // 左侧块

// 5. 结构化约束 (Transfinite)
Transfinite Curve {1, 3, 10} = nC;
Transfinite Curve {2, 4, 9, 11} = nL;
Transfinite Curve {5, 6, 7, 8} = nR;

Transfinite Surface {1, 2, 3, 4};
Recombine Surface {1, 2, 3, 4};

// 6. 扫掠 3D (Extrude) 
// 关键：分别拉伸每个面，并使用变量捕捉生成的体积 ID (v[1])
v1[] = Extrude {0, 0, H} { Surface{1}; Layers{nH}; Recombine; };
v2[] = Extrude {0, 0, H} { Surface{2}; Layers{nH}; Recombine; };
v3[] = Extrude {0, 0, H} { Surface{3}; Layers{nH}; Recombine; };
v4[] = Extrude {0, 0, H} { Surface{4}; Layers{nH}; Recombine; };

// 核心修改：将所有 4 个体积部分都纳入物理组，确保渲染和解析完整
Physical Volume("Half_Cylinder_Substance") = {v1[1], v2[1], v3[1], v4[1]};

// 7. 导出设置
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(radius).arg(height)
        .arg(nC).arg(nL).arg(nR).arg(nH)
        .arg(cx).arg(cy).arg(cz);
}