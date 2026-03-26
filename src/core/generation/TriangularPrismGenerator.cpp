#include "TriangularPrismGenerator.h"
#include <cmath>

QString TriangularPrismGenerator::buildGeoScript(double radius, double height, double meshSize, double cx, double cy, double cz) const {

    // 计算段数：nRad（径向），nH（高度）
    // 为了保证四边形质量，nRad 建议根据半径计算
    int nRad = std::max(2, (int)std::round(radius / meshSize));
    int nH = std::max(2, (int)std::round(height / meshSize));

    return QString(R"(
// =======================================================
// Triangular Prism Pure Hex Mesh (Three-block strategy)
// =======================================================

// 1. 基础参数
R = %1;  // 外接圆半径
H = %2;  // 高度
nRad = %3; // 径向等分数
nH = %4;   // 高度等分数
CX = %5; CY = %6; CZ = %7;

// 2. 点定义 (底面 Z=0)
Point(100) = {CX, CY, CZ}; // 中心点

// 3个顶点
Point(1) = {CX + R*Cos(Pi/2),          CY + R*Sin(Pi/2),          CZ};
Point(2) = {CX + R*Cos(Pi/2 + 2*Pi/3), CY + R*Sin(Pi/2 + 2*Pi/3), CZ};
Point(3) = {CX + R*Cos(Pi/2 + 4*Pi/3), CY + R*Sin(Pi/2 + 4*Pi/3), CZ};

// 3个边中点 (正三角形中心到边中点距离为 R*0.5)
Rm = R * 0.5;
Point(4) = {CX + Rm*Cos(Pi/2 + Pi/3),   CY + Rm*Sin(Pi/2 + Pi/3),   CZ};
Point(5) = {CX + Rm*Cos(Pi/2 + 3*Pi/3), CY + Rm*Sin(Pi/2 + 3*Pi/3), CZ};
Point(6) = {CX + Rm*Cos(Pi/2 + 5*Pi/3), CY + Rm*Sin(Pi/2 + 5*Pi/3), CZ};

// 3. 线段定义
Line(1) = {100, 4}; Line(2) = {100, 5}; Line(3) = {100, 6}; // 放射线
Line(4) = {1, 4};   Line(5) = {4, 2};                       // 边1
Line(6) = {2, 5};   Line(7) = {5, 3};                       // 边2
Line(8) = {3, 6};   Line(9) = {6, 1};                       // 边3

// 4. 构造 3 个四边形面
Curve Loop(1) = {1, -4, -9, -3}; Plane Surface(1) = {1};
Curve Loop(2) = {2, -6, -5, -1}; Plane Surface(2) = {2};
Curve Loop(3) = {3, -8, -7, -2}; Plane Surface(3) = {3};

// 5. 结构化约束
Transfinite Curve {1:9} = nRad;
Transfinite Surface {1, 2, 3};
Recombine Surface {1, 2, 3};

// 6. 挤压生成 3D 柱体
v_list[] = Extrude {0, 0, H} {
  Surface{1, 2, 3}; 
  Layers{nH}; 
  Recombine;
};

// 7. 物理组与导出设置
Physical Volume("TriPrism_Solid") = {1, 2, 3};
Mesh.RecombineAll = 1;
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; // 只导出六面体
Mesh 3;
)")
.arg(radius)
.arg(height)
.arg(nRad)
.arg(nH)
.arg(cx).arg(cy).arg(cz);
}