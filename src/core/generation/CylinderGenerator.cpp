#include "CylinderGenerator.h"

QString CylinderGenerator::buildGeoScript(double radius, double meshSize, double height, double cx, double cy, double cz) const {
    return QString(R"(
// 强制使用内置几何引擎，彻底杜绝 ID 分配错乱
SetFactory("Built-in");

// ============================================================================
// 1. 基础参数定义 (C++ 自动注入)
// ============================================================================
R  = %1;
H  = %2;
ms = %3;
CX = %4; 
CY = %5; 
CZ = %6;

// O-Grid 八边形核心比例
ratio = 0.707; 
L_in = R * ratio;

// ============================================================================
// 2. 自动网格分度算法 (保持长宽比 Aspect Ratio ~ 1:1:1)
// ============================================================================
// 环向分度：圆周被切割为8份(八边形)，单段弧长应为 2*Pi*R / 8 = Pi*R / 4
arc_len = Pi * R / 4.0;
nC = Max(1, Round(arc_len / ms));
actual_ms = arc_len / nC; // 真实弧长，作为后续网格的基准

// 径向与垂向分度 (以 actual_ms 为基准)
nR = Max(1, Round((R - L_in) / actual_ms));
nH = Max(1, Round(H / actual_ms));

// 转换为底层需要的节点数
nC_nodes = nC + 1;
nR_nodes = nR + 1;
nH_layers = nH; // Extrude 接收的是层数

// 计算八边形坐标偏移系数
A = L_in * Cos(Pi/8); B = L_in * Sin(Pi/8);
AR = R * Cos(Pi/8); BR = R * Sin(Pi/8);

// ============================================================================
// 3. 阵列化顶点生成
// ============================================================================
Point(0) = {CX, CY, CZ}; // 底面几何中心

// 内八边形顶点
Point(1) = {CX + A,  CY + B,  CZ}; Point(2) = {CX + B,  CY + A,  CZ}; 
Point(3) = {CX - B,  CY + A,  CZ}; Point(4) = {CX - A,  CY + B,  CZ};
Point(5) = {CX - A,  CY - B,  CZ}; Point(6) = {CX - B,  CY - A,  CZ}; 
Point(7) = {CX + B,  CY - A,  CZ}; Point(8) = {CX + A,  CY - B,  CZ};

// 外圆周顶点
Point(11)= {CX + AR, CY + BR, CZ}; Point(12)= {CX + BR, CY + AR, CZ}; 
Point(13)= {CX - BR, CY + AR, CZ}; Point(14)= {CX - AR, CY + BR, CZ};
Point(15)= {CX - AR, CY - BR, CZ}; Point(16)= {CX - BR, CY - AR, CZ};
Point(17)= {CX + BR, CY - AR, CZ}; Point(18)= {CX + AR, CY - BR, CZ};

// ============================================================================
// 4. 线框与面域拓扑生成
// ============================================================================
Line(101) = {0, 1}; Line(103) = {0, 3}; Line(105) = {0, 5}; Line(107) = {0, 7};
For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Line(i) = {i, ni};
    Line(20+i) = {i, 10+i};
    Circle(30+i) = {10+i, 0, 10+ni};
EndFor

// 中心 4 块平面
Curve Loop(1) = {101, 1, 2, -103}; Plane Surface(1) = {1}; 
Curve Loop(2) = {103, 3, 4, -105}; Plane Surface(2) = {2}; 
Curve Loop(3) = {105, 5, 6, -107}; Plane Surface(3) = {3}; 
Curve Loop(4) = {107, 7, 8, -101}; Plane Surface(4) = {4}; 

// 外围辐射 8 块平面
For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Curve Loop(10+i) = {20+i, 30+i, -(20+ni), -i}; Plane Surface(10+i) = {10+i}; 
EndFor

// 施加二维点阵约束
Transfinite Curve {101, 103, 105, 107} = nC_nodes;
Transfinite Curve {1:8, 31:38} = nC_nodes;
Transfinite Curve {21:28} = nR_nodes;

// ============================================================================
// 5. 一键 3D 拉伸与全量组装
// ============================================================================
// 将 12 个截面面一次性向上拉伸，生成 12 个实体体积
Extrude {0, 0, H} {
    Surface{1, 2, 3, 4, 11, 12, 13, 14, 15, 16, 17, 18};
    Layers{nH_layers};
    Recombine;
}

// 强制施加全局结构化约束
Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";

// 暴力抓取当前空间所有生成的体积 (避免 ID 错乱导致的空底面 Bug)
Physical Volume("Solid_Hex") = Volume "*";

Mesh.RecombineAll = 1;
Mesh.SaveAll = 0; 
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(radius).arg(height).arg(meshSize)
        .arg(cx).arg(cy).arg(cz);
}