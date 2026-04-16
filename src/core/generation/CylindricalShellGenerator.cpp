#include "CylindricalShellGenerator.h"

QString CylindricalShellGenerator::buildGeoScript(double radius, double height, double lid, double wall, double meshSize, double cx, double cy, double cz) const {
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

R_out  = R_in + Wall;
H_void = H_total - 2 * H_cap;

// O-Grid 核心比例
ratio = 0.707; 
L_in = R_in * ratio;

// ============================================================================
// 2. 自动网格分度算法 (保障 Aspect Ratio ~ 1)
// ============================================================================
// 2.1 环向分度 (以外圈周长为主，确保最外侧网格不至于太大)
// 圆周被切割为8份(八边形)，单段弧长应为 2*Pi*R / 8 = Pi*R / 4.0
arc_len = Pi * R_out / 4.0;
nC = Max(1, Round(arc_len / ms));
actual_ms = arc_len / nC; // 反推实际的环向弧长，作为后续网格的绝对基准尺寸

// 2.2 径向分度 (以实际弧长 actual_ms 为基准)
nR_in   = Max(1, Round((R_in - L_in) / actual_ms));
nR_wall = Max(1, Round(Wall / actual_ms));

// 2.3 垂向分度 (由于无锥度斜边=直边)
nH_cap  = Max(1, Round(H_cap / actual_ms));
nH_void = Max(1, Round(H_void / actual_ms));

// 转换为底层需要的节点数 (单元数 + 1)
nC_nodes      = nC + 1;
nR_in_nodes   = nR_in + 1;
nR_wall_nodes = nR_wall + 1;

// ============================================================================
// 3. 阵列化基础二维平面生成 (Z = CZ)
// ============================================================================
// 计算坐标偏置
A_in = L_in * Cos(Pi/8);   B_in = L_in * Sin(Pi/8);
AR_in = R_in * Cos(Pi/8);  BR_in = R_in * Sin(Pi/8);
AR_out= R_out * Cos(Pi/8); BR_out= R_out * Sin(Pi/8);

Point(0) = {CX, CY, CZ};

// 内八边形顶点 (1-8)
Point(1) = {CX + A_in,  CY + B_in, CZ}; Point(2) = {CX + B_in,  CY + A_in, CZ};
Point(3) = {CX - B_in,  CY + A_in, CZ}; Point(4) = {CX - A_in,  CY + B_in, CZ};
Point(5) = {CX - A_in, CY - B_in, CZ}; Point(6) = {CX - B_in, CY - A_in, CZ};
Point(7) = {CX + B_in, CY - A_in, CZ}; Point(8) = {CX + A_in, CY - B_in, CZ};

// 内腔圆周顶点 (11-18)
Point(11)= {CX + AR_in, CY + BR_in, CZ}; Point(12)= {CX + BR_in, CY + AR_in, CZ};
Point(13)= {CX - BR_in, CY + AR_in, CZ}; Point(14)= {CX - AR_in, CY + BR_in, CZ};
Point(15)= {CX - AR_in, CY - BR_in, CZ}; Point(16)= {CX - BR_in, CY - AR_in, CZ};
Point(17)= {CX + BR_in, CY - AR_in, CZ}; Point(18)= {CX + AR_in, CY - BR_in, CZ};

// 外壳圆周顶点 (21-28)
Point(21)= {CX + AR_out, CY + BR_out, CZ}; Point(22)= {CX + BR_out, CY + AR_out, CZ};
Point(23)= {CX - BR_out, CY + AR_out, CZ}; Point(24)= {CX - AR_out, CY + BR_out, CZ};
Point(25)= {CX - AR_out, CY - BR_out, CZ}; Point(26)= {CX - BR_out, CY - AR_out, CZ};
Point(27)= {CX + BR_out, CY - AR_out, CZ}; Point(28)= {CX + AR_out, CY - BR_out, CZ};

// --- 线段 ---
Line(101)={0,1}; Line(103)={0,3}; Line(105)={0,5}; Line(107)={0,7};

For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    // 内八边形边界
    Line(i) = {i, ni};
    // 核心辐射线
    Line(110+i) = {i, 10+i};
    // 内腔边界弧
    Circle(120+i) = {10+i, 0, 10+ni};
    // 壳体辐射线
    Line(130+i) = {10+i, 20+i};
    // 外壳边界弧
    Circle(140+i) = {20+i, 0, 20+ni};
EndFor

// --- 平面生成 ---
Curve Loop(1) = {101, 1, 2, -103}; Plane Surface(1) = {1}; Transfinite Surface {1} = {0, 1, 2, 3};
Curve Loop(2) = {103, 3, 4, -105}; Plane Surface(2) = {2}; Transfinite Surface {2} = {0, 3, 4, 5};
Curve Loop(3) = {105, 5, 6, -107}; Plane Surface(3) = {3}; Transfinite Surface {3} = {0, 5, 6, 7};
Curve Loop(4) = {107, 7, 8, -101}; Plane Surface(4) = {4}; Transfinite Surface {4} = {0, 7, 8, 1};

For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    // 核心到内腔过渡块
    Curve Loop(10+i) = {110+i, 120+i, -(110+ni), -i}; 
    Plane Surface(10+i) = {10+i};
    Transfinite Surface {10+i} = {i, 10+i, 10+ni, ni};
    
    // 壳体壁厚实体块
    Curve Loop(20+i) = {130+i, 140+i, -(130+ni), -(120+i)}; 
    Plane Surface(20+i) = {20+i};
    Transfinite Surface {20+i} = {10+i, 20+i, 20+ni, 10+ni};
EndFor

// 施加点阵约束
Transfinite Curve {101, 103, 105, 107} = nC_nodes;
Transfinite Curve {1:8, 121:128, 141:148} = nC_nodes;
Transfinite Curve {111:118} = nR_in_nodes;
Transfinite Curve {131:138} = nR_wall_nodes;

// 共 20 块表面合并
S_all[] = {1,2,3,4, 11,12,13,14,15,16,17,18, 21,22,23,24,25,26,27,28};
Recombine Surface {S_all[]};

// ============================================================================
// 4. 三层动态拉伸与剔除空腔
// ============================================================================
// 第一层：底部端盖
out1[] = Extrude {0, 0, H_cap} { Surface{S_all[]}; Layers{nH_cap}; Recombine; };

// 提取底端盖的顶面供第二层拉伸
S_top1[] = {};
For i In {0:19}
  S_top1[i] = out1[i*6]; 
EndFor

// 第二层：中间壁厚与空腔段
out2[] = Extrude {0, 0, H_void} { Surface{S_top1[]}; Layers{nH_void}; Recombine; };

// 提取中间层的顶面供第三层拉伸
S_top2[] = {};
For i In {0:19}
  S_top2[i] = out2[i*6];
EndFor

// 第三层：顶部端盖
out3[] = Extrude {0, 0, H_cap} { Surface{S_top2[]}; Layers{nH_cap}; Recombine; };

// ============================================================================
// 5. 递归删除中间层空腔与实体组装
// ============================================================================
// 删除第二层的前 12 个体积 (4个核心 + 8个过渡 = 中间空腔)
vols_to_delete[] = {};
For i In {0:11}
  vols_to_delete[i] = out2[i*6 + 1];
EndFor
Recursive Delete { Volume{vols_to_delete[]}; }

vols_solid[] = {};
// 追加底端盖实体 (20块)
For i In {0:19}
  vols_solid[#vols_solid[]] = out1[i*6 + 1];
EndFor
// 追加中间层外壁实体 (8块，索引 12~19)
For i In {12:19}
  vols_solid[#vols_solid[]] = out2[i*6 + 1];
EndFor
// 追加顶端盖实体 (20块)
For i In {0:19}
  vols_solid[#vols_solid[]] = out3[i*6 + 1];
EndFor

Physical Volume("Cylindrical_Shell_With_Caps") = {vols_solid[]};

Mesh.RecombineAll = 1;
Mesh.SaveAll = 0; 
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(radius).arg(height).arg(lid).arg(wall).arg(meshSize)
        .arg(cx).arg(cy).arg(cz);
}