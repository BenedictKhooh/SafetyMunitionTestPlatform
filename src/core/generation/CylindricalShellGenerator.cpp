#include "CylindricalShellGenerator.h"

QString CylindricalShellGenerator::buildGeoScript(double radius, double height, double lid, double wall, double meshSize) const {
    // --- 在 C++ 中预计算 Gmsh 脚本所需的离散分段数 ---
    // nC: 周向分段, nH_cap: 端盖高度分段, nR_wall: 壁厚分段
    int nC = 2 * std::round((radius / (2.0 * 1.414)) / meshSize);
    if (nC < 2) nC = 2;

    int nH_cap = std::round(lid / meshSize);
    if (nH_cap < 1) nH_cap = 1;

    int nR_wall = std::round(wall / meshSize);
    if (nR_wall < 1) nR_wall = 1;

    // 中间空腔部分的高度分段数
    double voidHeight = height - 2.0 * lid;
    int nH_void = std::round(voidHeight / meshSize);
    if (nH_void < 1) nH_void = 1;

    return QString(R"(
// --- 1. 参数定义 ---
R_in = %1; 
L_val = R_in / (2 * 1.414); 
Wall = %2;
R_out = R_in + Wall;
H_height = %3;
H_cap = %4; 
H_void = H_height - 2 * H_cap;

nC = %5; 
nR_in = Round(nC / 1.414); 
nR_wall = %6; 
nH_cap = %7; 
nH_void = %8;

// --- 2. 基础面定义 (Z=0) ---
Point(1) = {0, 0, 0}; 
Point(2) = {L_val, L_val, 0};    Point(3) = {-L_val, L_val, 0};
Point(4) = {-L_val, -L_val, 0};  Point(5) = {L_val, -L_val, 0};
p = R_in / 1.414;
Point(6) = {p, p, 0};    Point(7) = {-p, p, 0};
Point(8) = {-p, -p, 0};  Point(9) = {p, -p, 0};
p2 = R_out / 1.414;
Point(10) = {p2, p2, 0}; Point(11) = {-p2, p2, 0};
Point(12) = {-p2, -p2, 0}; Point(13) = {p2, -p2, 0};

Line(1)={2,3}; Line(2)={3,4}; Line(3)={4,5}; Line(4)={5,2};
Line(5)={2,6}; Line(6)={3,7}; Line(7)={4,8}; Line(8)={5,9};
Circle(9)={6,1,7}; Circle(10)={7,1,8}; Circle(11)={8,1,9}; Circle(12)={9,1,6};
Line(13)={6,10}; Line(14)={7,11}; Line(15)={8,12}; Line(16)={9,13};
Circle(17)={10,1,11}; Circle(18)={11,1,12}; Circle(19)={12,1,13}; Circle(20)={13,1,10};

Curve Loop(101) = {1, 2, 3, 4};            Plane Surface(1) = {101}; 
Curve Loop(102) = {5, 9, -6, -1};          Plane Surface(2) = {102}; 
Curve Loop(103) = {6, 10, -7, -2};         Plane Surface(3) = {103};
Curve Loop(104) = {7, 11, -8, -3};         Plane Surface(4) = {104};
Curve Loop(105) = {8, 12, -5, -4};         Plane Surface(5) = {105};
Curve Loop(106) = {13, 17, -14, -9};       Plane Surface(6) = {106}; 
Curve Loop(107) = {14, 18, -15, -10};      Plane Surface(7) = {107};
Curve Loop(108) = {15, 19, -16, -11};      Plane Surface(8) = {108};
Curve Loop(109) = {16, 20, -13, -12};      Plane Surface(9) = {109};

Transfinite Surface {1:9}; Recombine Surface {1:9};
Transfinite Curve {1:4, 9:12, 17:20} = nC;
Transfinite Curve {5:8} = nR_in; Transfinite Curve {13:16} = nR_wall;

// --- 3. 顺序拉伸 ---
// 第一层：底部端盖
out1[] = Extrude {0, 0, H_cap} { Surface{1:9}; Layers{nH_cap}; Recombine; };

// 第二层：中间层
out2[] = Extrude {0, 0, H_void} { 
  Surface{out1[0], out1[6], out1[12], out1[18], out1[24], out1[30], out1[36], out1[42], out1[48]}; 
  Layers{nH_void}; Recombine; 
};

// 第三层：顶部端盖
out3[] = Extrude {0, 0, H_cap} { 
  Surface{out2[0], out2[6], out2[12], out2[18], out2[24], out2[30], out2[36], out2[42], out2[48]}; 
  Layers{nH_cap}; Recombine; 
};

// --- 4. 递归删除中间空腔部分的体积 ---
Recursive Delete {
  Volume{out2[1], out2[7], out2[13], out2[19], out2[25]};
}

// --- 5. 重新约束顶部体积 ---
Transfinite Volume {out3[1], out3[7], out3[13], out3[19], out3[25], out3[31], out3[37], out3[43], out3[49]};

// --- 6. 物理组定义 ---
Physical Volume("Shell_Solid") = {
  out1[1], out1[7], out1[13], out1[19], out1[25], out1[31], out1[37], out1[43], out1[49],
  out2[31], out2[37], out2[43], out2[49],
  out3[1], out3[7], out3[13], out3[19], out3[25], out3[31], out3[37], out3[43], out3[49]
};

Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(radius).arg(wall).arg(height).arg(lid)
        .arg(nC).arg(nR_wall).arg(nH_cap).arg(nH_void);
}