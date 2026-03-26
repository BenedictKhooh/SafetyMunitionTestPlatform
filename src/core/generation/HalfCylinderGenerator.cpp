#include "HalfCylinderGenerator.h"
#include <QtMath>

QString HalfCylinderGenerator::buildGeoScript(double radius, double height, double meshSize, double cx, double cy, double cz) const {

    double Rproj_val = radius / 1.41421356;
    double L_val = Rproj_val / 2.0;

    // 自动计算分段数 (+1 是因为 Transfinite 算的是节点数)
    int nC = qMax(2, (int)qRound(Rproj_val / meshSize)) + 1;
    int nL = qMax(2, (int)qRound(L_val / meshSize)) + 1;
    int nR = qMax(2, (int)qRound((radius - L_val) / meshSize)) + 1;
    int nH = qMax(2, (int)qRound(height / meshSize)); // Layers 里填的是段数，不加1

    return QString(R"(
// 1. 参数定义
R = %1; H = %2;
nC = %3; nL = %4; nR = %5; nH = %6;
CX = %7; CY = %8; CZ = %9;

Rproj = R / 1.41421356; 
L = Rproj / 2.0; 

// 2. 点定义
Point(1) = {CX + 0, CY + 0, CZ + 0};
Point(2) = {CX - L, CY + 0, CZ + 0};
Point(3) = {CX + L, CY + 0, CZ + 0};
Point(4) = {CX + L, CY + L, CZ + 0};
Point(5) = {CX - L, CY + L, CZ + 0};

Point(6) = {CX - R,     CY + 0,     CZ + 0};
Point(7) = {CX + R,     CY + 0,     CZ + 0};
Point(8) = {CX + Rproj, CY + Rproj, CZ + 0};
Point(9) = {CX - Rproj, CY + Rproj, CZ + 0};

// 3. 线定义
Line(1) = {2, 3}; Line(2) = {3, 4}; Line(3) = {4, 5}; Line(4) = {5, 2}; 
Line(5) = {6, 2}; Line(6) = {3, 7}; Line(7) = {4, 8}; Line(8) = {5, 9}; 
Circle(9)  = {7, 1, 8}; Circle(10) = {8, 1, 9}; Circle(11) = {9, 1, 6}; 

// 4. 面定义
Curve Loop(1) = {1, 2, 3, 4};        Plane Surface(1) = {1};
Curve Loop(2) = {6, 9, -7, -2};      Plane Surface(2) = {2};
Curve Loop(3) = {7, 10, -8, -3};     Plane Surface(3) = {3};
Curve Loop(4) = {8, 11, 5, -4};      Plane Surface(4) = {4};

// 5. 结构化约束
Transfinite Curve {1, 3, 10} = nC;
Transfinite Curve {2, 4, 9, 11} = nL;
Transfinite Curve {5, 6, 7, 8} = nR;

Transfinite Surface {1, 2, 3, 4};
Recombine Surface {1, 2, 3, 4};

// 6. 扫掠 3D (整体拉伸保证节点连接)
Extrude {0, 0, H} {
  Surface{1, 2, 3, 4};
  Layers{nH};
  Recombine;
}

// 🌟 物理组抓取：使用 Volume "*" 一网打尽所有体积，杜绝漏抓
Physical Volume("Half_Cylinder_Solid") = Volume "*";

// 🌟 导出安全锁：强制使用 2.2 格式，保护 C++ 解析器不崩溃
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; 
Mesh 3;
)")
.arg(radius).arg(height)
.arg(nC).arg(nL).arg(nR).arg(nH)
.arg(cx).arg(cy).arg(cz);
}