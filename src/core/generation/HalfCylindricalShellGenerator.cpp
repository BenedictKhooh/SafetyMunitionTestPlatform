#include "HalfCylindricalShellGenerator.h"
#include <QtMath>

QString HalfCylindricalShellGenerator::buildGeoScript(double radius, double height, double lid, double wall, double meshSize,
    double cx, double cy, double cz) const {

    // --- 1. 预计算离散分段数 ---
    // nC: 周向分段 (90度对应 Rproj/ms)
    int nC = qMax(2, (int)qRound((radius / 1.414) / meshSize));

    // nR_wall: 壁厚分段
    int nR_wall = qMax(1, (int)qRound(wall / meshSize));

    // nH_cap: 端盖分段
    int nH_cap = qMax(1, (int)qRound(lid / meshSize));

    // nH_void: 空腔高度分段
    double voidHeight = height - 2.0 * lid;
    int nH_void = qMax(1, (int)qRound(voidHeight / meshSize));

    return QString(R"(
////////////////////////////////////////////////////
// Half-Cylindrical Shell (Parameterized & Offset)
////////////////////////////////////////////////////

// --- 1. 参数与几何计算 ---
R_in = %1; Wall = %2; H_height = %3; H_cap = %4;
R_out = R_in + Wall;
H_void = H_height - 2 * H_cap;

nC = %5; nR_wall = %6; nH_cap = %7; nH_void = %8;
CX = %9; CY = %10; CZ = %11;

L_in = R_in / (2 * 1.414);
p_in = R_in / 1.414;
p_out = R_out / 1.414;

// --- 2. 基础面定义 (Z=0) ---
Point(1) = {CX + 0, CY + 0, CZ + 0}; 

Point(2) = {CX - L_in, CY + 0,    CZ + 0}; Point(3) = {CX + L_in, CY + 0,    CZ + 0};
Point(4) = {CX + L_in, CY + L_in, CZ + 0}; Point(5) = {CX - L_in, CY + L_in, CZ + 0};
Point(6) = {CX - R_in, CY + 0,    CZ + 0}; Point(7) = {CX + R_in, CY + 0,    CZ + 0};
Point(8) = {CX + p_in, CY + p_in, CZ + 0}; Point(9) = {CX - p_in, CY + p_in, CZ + 0};

Point(10) = {CX - R_out, CY + 0,     CZ + 0}; Point(11) = {CX + R_out, CY + 0,     CZ + 0};
Point(12) = {CX + p_out, CY + p_out, CZ + 0}; Point(13) = {CX - p_out, CY + p_out, CZ + 0};

Line(1)={2,3}; Line(2)={3,4}; Line(3)={4,5}; Line(4)={5,2};
Line(5)={6,2}; Line(6)={3,7}; Line(7)={4,8}; Line(8)={5,9};
Circle(9)={7,1,8}; Circle(10)={8,1,9}; Circle(11)={9,1,6};

Line(12)={7,11}; Line(13)={8,12}; Line(14)={9,13}; Line(15)={6,10};
Circle(16)={11,1,12}; Circle(17)={12,1,13}; Circle(18)={13,1,10};

// 面定义: 1-4 内部, 5-7 壁厚
Curve Loop(101)={1,2,3,4};    Plane Surface(1)={101};
Curve Loop(102)={6,9,-7,-2};  Plane Surface(2)={102};
Curve Loop(103)={7,10,-8,-3}; Plane Surface(3)={103};
Curve Loop(104)={8,11,5,-4};  Plane Surface(4)={104};
Curve Loop(105)={12,16,-13,-9}; Plane Surface(5)={105};
Curve Loop(106)={13,17,-14,-10};Plane Surface(6)={106};
Curve Loop(107)={14,18,-15,-11};Plane Surface(7)={107};

Transfinite Surface {1:7}; Recombine Surface {1:7};
Transfinite Curve {1, 3, 10, 17} = nC;
Transfinite Curve {2, 4, 9, 11, 16, 18} = Max(2, nC/2);
Transfinite Curve {5, 6, 7, 8} = nC; 
Transfinite Curve {12, 13, 14, 15} = nR_wall;

// --- 3. 顺序拉伸 ---
// 第一层：底部端盖
out1[] = Extrude {0, 0, H_cap} { Surface{1:7}; Layers{nH_cap}; Recombine; };

// 第二层：中间空腔层
out2[] = Extrude {0, 0, H_void} { 
    Surface{out1[0], out1[6], out1[12], out1[18], out1[24], out1[30], out1[36]}; 
    Layers{nH_void}; Recombine; 
};

// 第三层：顶部端盖
out3[] = Extrude {0, 0, H_cap} { 
    Surface{out2[0], out2[6], out2[12], out2[18], out2[24], out2[30], out2[36]}; 
    Layers{nH_cap}; Recombine; 
};

// --- 4. 挖空中间层 ---
Recursive Delete { Volume{out2[1], out2[7], out2[13], out2[19]}; }

// --- 5. 物理组 ---
Physical Volume("Half_Shell_Solid") = {
    out1[1], out1[7], out1[13], out1[19], out1[25], out1[31], out1[37],
    out2[25], out2[31], out2[37],
    out3[1], out3[7], out3[13], out3[19], out3[25], out3[31], out3[37]
};

Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(radius).arg(wall).arg(height).arg(lid)
        .arg(nC).arg(nR_wall).arg(nH_cap).arg(nH_void)
        .arg(cx).arg(cy).arg(cz);
}