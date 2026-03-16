#include "HexagonalPrismGenerator.h"
#include <cmath>
#include <algorithm>

QString HexagonalPrismGenerator::buildGeoScript(double radius, double height, double meshSize, double cx, double cy, double cz) const {

    // 根据 meshSize 自动计算边长和高度的等分数
    // 确保 n 至少为 2
    int n = std::max(2, (int)std::round(radius / meshSize));
    int nH = std::max(2, (int)std::round(height / meshSize));

    return QString(R"(
// =======================================================
// Hexagonal Prism Pure Hex Mesh (Three-block strategy)
// =======================================================

// 1. 基础参数
R = %1;  
H = %2;  
n = %3;   // 边长等分数
nH = %4;  // 高度等分数
CX = %5; CY = %6; CZ = %7;

// 2. 点定义 (底面 Z=CZ)
Point(100) = {CX, CY, CZ}; // 中心点

Point(1) = {CX + R*Cos(0),           CY + R*Sin(0),           CZ};
Point(2) = {CX + R*Cos(1*Pi/3),      CY + R*Sin(1*Pi/3),      CZ};
Point(3) = {CX + R*Cos(2*Pi/3),      CY + R*Sin(2*Pi/3),      CZ};
Point(4) = {CX + R*Cos(3*Pi/3),      CY + R*Sin(3*Pi/3),      CZ};
Point(5) = {CX + R*Cos(4*Pi/3),      CY + R*Sin(4*Pi/3),      CZ};
Point(6) = {CX + R*Cos(5*Pi/3),      CY + R*Sin(5*Pi/3),      CZ};

// 3. 线段定义
Line(1) = {1, 2}; 
Line(2) = {2, 3}; 
Line(3) = {3, 4}; 
Line(4) = {4, 5}; 
Line(5) = {5, 6}; 
Line(6) = {6, 1};

Line(7) = {100, 2}; // 中心 -> 2
Line(8) = {100, 4}; // 中心 -> 4
Line(9) = {100, 6}; // 中心 -> 6

// 4. 构造 3 个封闭的四边形面
Curve Loop(1) = {6, 1, -7, 9};    Plane Surface(1) = {1};
Curve Loop(2) = {2, 3, -8, 7};    Plane Surface(2) = {2};
Curve Loop(3) = {4, 5, -9, 8};    Plane Surface(3) = {3};

// 5. 结构化约束
Transfinite Curve {1:9} = n;
Transfinite Surface {1, 2, 3};
Recombine Surface {1, 2, 3};

// 6. 挤压生成 3D 柱体
v_all[] = Extrude {0, 0, H} {
  Surface{1, 2, 3}; 
  Layers{nH}; 
  Recombine;
};

// 7. 物理组定义 (强制只导出 Type 5 六面体)
Physical Volume("Hexa_Solid") = {v_all[1], v_all[6], v_all[11]};

// 8. 网格生成控制
Mesh.RecombineAll = 1;
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; 
Mesh 3;
)")
.arg(radius)
.arg(height)
.arg(n)
.arg(nH)
.arg(cx).arg(cy).arg(cz);
}