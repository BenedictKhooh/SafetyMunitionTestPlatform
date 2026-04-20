#include "CylindricalShellGenerator.h"

QString CylindricalShellGenerator::buildGeoScript(double radius, double height, double lid, double wall, double meshSize, double cx, double cy, double cz, double msWall) const {
    return QString(R"(
// 开启 OpenCASCADE 引擎，完美支持 3D 逻辑切割与阵列
SetFactory("OpenCASCADE");

// ============================================================================
// 1. 基础参数定义 (C++ 自动注入)
// ============================================================================
R_in   = %1;
H_total= %2;
H_cap  = %3;
Wall   = %4;
ms     = %5;
CX     = %6; 
CY     = %7; 
CZ     = %8;
ms_wall= %9; // ★ 新增：壁厚局部网格尺寸

R_out  = R_in + Wall;
H_void = H_total - 2 * H_cap;

// O-Grid 核心比例
ratio = 0.707; 
L_in = R_in * ratio;

// ============================================================================
// 2. 自动网格分度算法 (保障 Aspect Ratio ~ 1)
// ============================================================================
arc_len = Pi * R_out / 4.0;
nC = Max(1, Round(arc_len / ms));
actual_ms = arc_len / nC; 

// O-Grid 内部径向分度
nR_in = Max(1, Round((R_in - L_in) / actual_ms));

// ★ 核心修改：壁厚的分段数使用专属的 ms_wall 计算 ★
nR_wall = Max(1, Round(Wall / ms_wall));

// 节点数转换
nC_nodes      = nC + 1;
nR_in_nodes   = nR_in + 1;
nR_wall_nodes = nR_wall + 1;

nH_cap  = Max(1, Round(H_cap / ms));
nH_void = Max(1, Round(H_void / ms));

// ============================================================================
// 3. 阵列化顶点与线框生成 (底面 2D)
// ============================================================================
Point(0) = {CX, CY, CZ}; 

A_in = L_in * Cos(Pi/8);   B_in = L_in * Sin(Pi/8);
AR_in = R_in * Cos(Pi/8);  BR_in = R_in * Sin(Pi/8);
AR_out = R_out * Cos(Pi/8);BR_out = R_out * Sin(Pi/8);

Point(1) = {CX + A_in, CY + B_in, CZ}; Point(2) = {CX + B_in, CY + A_in, CZ}; 
Point(3) = {CX - B_in, CY + A_in, CZ}; Point(4) = {CX - A_in, CY + B_in, CZ};
Point(5) = {CX - A_in, CY - B_in, CZ}; Point(6) = {CX - B_in, CY - A_in, CZ}; 
Point(7) = {CX + B_in, CY - A_in, CZ}; Point(8) = {CX + A_in, CY - B_in, CZ};

Point(11) = {CX + AR_in, CY + BR_in, CZ}; Point(12) = {CX + BR_in, CY + AR_in, CZ}; 
Point(13) = {CX - BR_in, CY + AR_in, CZ}; Point(14) = {CX - AR_in, CY + BR_in, CZ};
Point(15) = {CX - AR_in, CY - BR_in, CZ}; Point(16) = {CX - BR_in, CY - AR_in, CZ};
Point(17) = {CX + BR_in, CY - AR_in, CZ}; Point(18) = {CX + AR_in, CY - BR_in, CZ};

Point(21) = {CX + AR_out, CY + BR_out, CZ}; Point(22) = {CX + BR_out, CY + AR_out, CZ}; 
Point(23) = {CX - BR_out, CY + AR_out, CZ}; Point(24) = {CX - AR_out, CY + BR_out, CZ};
Point(25) = {CX - AR_out, CY - BR_out, CZ}; Point(26) = {CX - BR_out, CY - AR_out, CZ};
Point(27) = {CX + BR_out, CY - AR_out, CZ}; Point(28) = {CX + AR_out, CY - BR_out, CZ};

Line(101) = {0, 1}; Line(103) = {0, 3}; Line(105) = {0, 5}; Line(107) = {0, 7};
For i In {1:8}
  ni = (i==8) ? 1 : i+1;
  Line(i) = {i, ni};
  Line(10+i) = {i, 10+i};
  Circle(20+i) = {10+i, 0, 10+ni};
  Line(30+i) = {10+i, 20+i};
  Circle(40+i) = {20+i, 0, 20+ni};
EndFor

Curve Loop(1) = {101, 1, 2, -103}; Plane Surface(1) = {1}; 
Curve Loop(2) = {103, 3, 4, -105}; Plane Surface(2) = {2}; 
Curve Loop(3) = {105, 5, 6, -107}; Plane Surface(3) = {3}; 
Curve Loop(4) = {107, 7, 8, -101}; Plane Surface(4) = {4}; 

For i In {1:8}
  ni = (i==8) ? 1 : i+1;
  Curve Loop(10+i) = {10+i, 20+i, -(10+ni), -i}; Plane Surface(10+i) = {10+i}; 
  Curve Loop(20+i) = {30+i, 40+i, -(30+ni), -(20+i)}; Plane Surface(20+i) = {20+i}; 
EndFor

// ============================================================================
// 4. 施加二维点阵约束
// ============================================================================
Transfinite Curve {101, 103, 105, 107} = nC_nodes;
Transfinite Curve {1:8, 21:28, 41:48}  = nC_nodes;
Transfinite Curve {11:18} = nR_in_nodes;
// ★ 壁厚约束点数此时已使用了 ms_wall 进行的高密度划分 ★
Transfinite Curve {31:38} = nR_wall_nodes;

Transfinite Surface "*"; 
Recombine Surface "*";

S_all[] = {1,2,3,4, 11,12,13,14,15,16,17,18, 21,22,23,24,25,26,27,28};

// ============================================================================
// 5. 三段式 3D 拉伸与空腔挖掘
// ============================================================================
out1[] = Extrude {0, 0, H_cap} { Surface{S_all[]}; Layers{nH_cap}; Recombine; };

S_top1[] = {};
For i In {0:19}
  S_top1[i] = out1[i*6]; 
EndFor

out2[] = Extrude {0, 0, H_void} { Surface{S_top1[]}; Layers{nH_void}; Recombine; };

S_top2[] = {};
For i In {0:19}
  S_top2[i] = out2[i*6];
EndFor

out3[] = Extrude {0, 0, H_cap} { Surface{S_top2[]}; Layers{nH_cap}; Recombine; };

vols_to_delete[] = {};
For i In {0:11}
  vols_to_delete[i] = out2[i*6 + 1];
EndFor
Delete { Volume{vols_to_delete[]}; }

Transfinite Volume "*";
Recombine Volume "*";
Physical Volume("Shell_Solid") = Volume "*";

Mesh.RecombineAll = 1;
Mesh.SurfaceEdges = 1;
Mesh.VolumeEdges = 1;

Mesh 3;
)").arg(radius).arg(height).arg(lid).arg(wall).arg(meshSize).arg(cx).arg(cy).arg(cz).arg(msWall);
}