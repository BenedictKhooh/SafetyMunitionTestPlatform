#include "OpenCylindricalShellGenerator.h"

QString OpenCylindricalShellGenerator::buildGeoScript(double radius, double wall, double h_base, double h_wall,
    double meshSize, double cx, double cy, double cz, double msWall) const {

    // 核心修改：利用专属的 msWall 控制壁厚的单元数
    double actualMsWall = (msWall > 1e-5) ? msWall : meshSize;

    int nC = 2 * std::max(1.0, std::round((radius / (2.0 * 1.414)) / meshSize));
    int nR_in = std::max(1, (int)std::round(nC / 1.414));
    // ★ 这里的除数变成了 actualMsWall
    int nR_wall = std::max(1, (int)std::round(wall / actualMsWall));

    int nH_base = std::max(1, (int)std::round(h_base / meshSize));
    int nH_wall = std::max(1, (int)std::round(h_wall / meshSize));

    return QString(R"(
// --- 1. 参数定义 ---
R_in = %1; Wall = %2; H_base = %3; H_wall = %4;
CX = %10; CY = %11; CZ = %12; // 起始点坐标传入 

R_out = R_in + Wall;
L_val = R_in / (2 * 1.414);
nC = %5; nR_in = %6; nR_wall = %7; nH_base = %8; nH_wall = %9;

// --- 2. 基础面定义 (O-Grid) ---
Point(0) = {CX, CY, CZ}; 
Point(1) = {CX + L_val, CY + L_val, CZ};   Point(2) = {CX - L_val, CY + L_val, CZ};
Point(3) = {CX - L_val, CY - L_val, CZ};   Point(4) = {CX + L_val, CY - L_val, CZ};

Rproj_in = R_in / 1.414;
Point(5) = {CX + Rproj_in, CY + Rproj_in, CZ}; Point(6) = {CX - Rproj_in, CY + Rproj_in, CZ};
Point(7) = {CX - Rproj_in, CY - Rproj_in, CZ}; Point(8) = {CX + Rproj_in, CY - Rproj_in, CZ};

Rproj_out = R_out / 1.414;
Point(9) = {CX + Rproj_out, CY + Rproj_out, CZ}; Point(10) = {CX - Rproj_out, CY + Rproj_out, CZ};
Point(11) = {CX - Rproj_out, CY - Rproj_out, CZ}; Point(12) = {CX + Rproj_out, CY - Rproj_out, CZ};

Line(1) = {1, 2}; Line(2) = {2, 3}; Line(3) = {3, 4}; Line(4) = {4, 1};
Line(5) = {1, 5}; Line(6) = {2, 6}; Line(7) = {3, 7}; Line(8) = {4, 8};
Circle(9) = {5, 0, 6}; Circle(10) = {6, 0, 7}; Circle(11) = {7, 0, 8}; Circle(12) = {8, 0, 5};
Line(13) = {5, 9}; Line(14) = {6, 10}; Line(15) = {7, 11}; Line(16) = {8, 12};
Circle(17) = {9, 0, 10}; Circle(18) = {10, 0, 11}; Circle(19) = {11, 0, 12}; Circle(20) = {12, 0, 9};

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
out1[] = Extrude {0, 0, H_base} { Surface{1:9}; Layers{nH_base}; Recombine; };
out2[] = Extrude {0, 0, H_wall} { Surface{out1[0], out1[6], out1[12], out1[18], out1[24], out1[30], out1[36], out1[42], out1[48]}; Layers{nH_wall}; Recombine; };

// 开口空腔剔除：删除中心和过渡区 (out2的第1到第5个面对应的拉伸体)
Delete { Volume{out2[1], out2[7], out2[13], out2[19], out2[25]}; }

Physical Volume("OpenCylindricalShell") = Volume "*";
Mesh 3;
)").arg(radius).arg(wall).arg(h_base).arg(h_wall).arg(nC).arg(nR_in).arg(nR_wall).arg(nH_base).arg(nH_wall).arg(cx).arg(cy).arg(cz);
}