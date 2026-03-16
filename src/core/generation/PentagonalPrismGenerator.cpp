#include "PentagonalPrismGenerator.h"
#include <cmath>
#include <algorithm>

QString PentagonalPrismGenerator::buildGeoScript(double radius, double height, double meshSize, double cx, double cy, double cz) const {

    // 1. 在 C++ 层预计算等分数，避免 Gmsh 脚本语法兼容性问题
    int nVal = std::max(2, (int)std::round(radius / meshSize));
    int nHVal = std::max(2, (int)std::round(height / meshSize));

    return QString(R"(
// =======================================================
// Pentagonal Prism - Pure Hex Meshing (5-Block Strategy)
// Strategy: Center to edge midpoints -> 5 Quadrilaterals
// =======================================================

Delete All; // 清除缓存

// --- 1. 参数定义 ---
R = %1; H = %2; n = %3; nH = %4;
CX = %5; CY = %6; CZ = %7;

// --- 2. 点定义 ---
Point(0) = {CX, CY, CZ}; // 中心

// 5个顶点 (1-5)
Point(1) = {CX + R*Cos(0),           CY + R*Sin(0),           CZ};
Point(2) = {CX + R*Cos(2*Pi/5),      CY + R*Sin(2*Pi/5),      CZ};
Point(3) = {CX + R*Cos(4*Pi/5),      CY + R*Sin(4*Pi/5),      CZ};
Point(4) = {CX + R*Cos(6*Pi/5),      CY + R*Sin(6*Pi/5),      CZ};
Point(5) = {CX + R*Cos(8*Pi/5),      CY + R*Sin(8*Pi/5),      CZ};

// 5个边中点 (6-10)
Rm = R * Cos(Pi/5); 
Point(6) = {CX + Rm*Cos(Pi/5),       CY + Rm*Sin(Pi/5),       CZ};
Point(7) = {CX + Rm*Cos(3*Pi/5),      CY + Rm*Sin(3*Pi/5),      CZ};
Point(8) = {CX + Rm*Cos(5*Pi/5),      CY + Rm*Sin(5*Pi/5),      CZ};
Point(9) = {CX + Rm*Cos(7*Pi/5),      CY + Rm*Sin(7*Pi/5),      CZ};
Point(10) = {CX + Rm*Cos(9*Pi/5),     CY + Rm*Sin(9*Pi/5),     CZ};

// --- 3. 线定义 ---
Line(1) = {0, 6}; Line(2) = {0, 7}; Line(3) = {0, 8}; Line(4) = {0, 9}; Line(5) = {0, 10};
Line(6) = {6, 2}; Line(7) = {2, 7}; Line(8) = {7, 3}; Line(9) = {3, 8}; Line(10) = {8, 4};
Line(11) = {4, 9}; Line(12) = {9, 5}; Line(13) = {5, 10}; Line(14) = {10, 1}; Line(15) = {1, 6};

// --- 4. 面定义 ---
Curve Loop(1) = {1, 6, 7, -2};   Plane Surface(1) = {1};
Curve Loop(2) = {2, 8, 9, -3};   Plane Surface(2) = {2};
Curve Loop(3) = {3, 10, 11, -4}; Plane Surface(3) = {3};
Curve Loop(4) = {4, 12, 13, -5}; Plane Surface(4) = {4};
Curve Loop(5) = {5, 14, 15, -1}; Plane Surface(5) = {5};

// --- 5. 网格约束 (结构化) ---
Transfinite Curve {1:15} = n;
Transfinite Surface {1:5};
Recombine Surface {1:5};

// --- 6. 分块挤压并捕捉 Volume ID ---
v1[] = Extrude {0, 0, H} { Surface{1}; Layers{nH}; Recombine; };
v2[] = Extrude {0, 0, H} { Surface{2}; Layers{nH}; Recombine; };
v3[] = Extrude {0, 0, H} { Surface{3}; Layers{nH}; Recombine; };
v4[] = Extrude {0, 0, H} { Surface{4}; Layers{nH}; Recombine; };
v5[] = Extrude {0, 0, H} { Surface{5}; Layers{nH}; Recombine; };

// --- 7. 物理组 (关键：导出所有块) ---
Physical Volume("Pentagonal_Prism_Hex") = {v1[1], v2[1], v3[1], v4[1], v5[1]};

// --- 8. 设置 ---
Mesh.RecombineAll = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
)")
.arg(radius).arg(height).arg(nVal).arg(nHVal)
.arg(cx).arg(cy).arg(cz);
}