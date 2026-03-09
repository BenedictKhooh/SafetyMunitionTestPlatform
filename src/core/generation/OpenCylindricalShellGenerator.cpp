#include "OpenCylindricalShellGenerator.h"

QString OpenCylindricalShellGenerator::buildGeoScript(double radius, double wall, double h_base, double h_wall,
    double meshSize, double cx, double cy, double cz) const {
    int nC = 2 * std::max(1.0, std::round((radius / (2.0 * 1.414)) / meshSize));
    int nR_in = std::max(1, (int)std::round(nC / 1.414));
    int nR_wall = std::max(1, (int)std::round(wall / meshSize));
    int nH_base = std::max(1, (int)std::round(h_base / meshSize));
    int nH_wall = std::max(1, (int)std::round(h_wall / meshSize));

    return QString(R"(
// --- 1. 参数定义 ---
R_in = %1; Wall = %2; H_base = %3; H_wall = %4;
CX = %10; CY = %11; CZ = %12; // 起始点坐标传入 

R_out = R_in + Wall;
L_val = R_in / (2 * 1.414);
nC = %5; nR_in = %6; nR_wall = %7; nH_base = %8; nH_wall = %9;

// --- 2. 基础面定义 (所有点坐标叠加偏移量) ---
Point(1) = {CX, CY, CZ}; 
Point(2) = {CX + L_val, CY + L_val, CZ};    Point(3) = {CX - L_val, CY + L_val, CZ};
Point(4) = {CX - L_val, CY - L_val, CZ};    Point(5) = {CX + L_val, CY - L_val, CZ};

p = R_in / 1.414;
Point(6) = {CX + p, CY + p, CZ};    Point(7) = {CX - p, CY + p, CZ};
Point(8) = {CX - p, CY - p, CZ};    Point(9) = {CX + p, CY - p, CZ};

p2 = R_out / 1.414;
Point(10) = {CX + p2, CY + p2, CZ}; Point(11) = {CX - p2, CY + p2, CZ};
Point(12) = {CX - p2, CY - p2, CZ}; Point(13) = {CX + p2, CY - p2, CZ};

// --- 线、面定义 (维持逻辑不变) ---
Line(1)={2,3}; Line(2)={3,4}; Line(3)={4,5}; Line(4)={5,2};
Line(5)={2,6}; Line(6)={3,7}; Line(7)={4,8}; Line(8)={5,9};
Circle(9)={6,1,7}; Circle(10)={7,1,8}; Circle(11)={8,1,9}; Circle(12)={9,1,6};
Line(13)={6,10}; Line(14)={7,11}; Line(15)={8,12}; Line(16)={9,13};
Circle(17)={10,1,11}; Circle(18)={11,1,12}; Circle(19)={12,1,13}; Circle(20)={13,1,10};

Curve Loop(101) = {1:4};                   Plane Surface(1) = {101}; 
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

// --- 3. 顺序拉伸 (这里高度正负决定方向) ---
out1[] = Extrude {0, 0, -H_base} { Surface{1:9}; Layers{nH_base}; Recombine; };
out2[] = Extrude {0, 0, -H_wall} { 
  Surface{out1[0], out1[6], out1[12], out1[18], out1[24], out1[30], out1[36], out1[42], out1[48]}; 
  Layers{nH_wall}; Recombine; 
};

Recursive Delete { Volume{out2[1], out2[7], out2[13], out2[19], out2[25]}; }

Physical Volume("Solid_Cup") = {
  out1[1], out1[7], out1[13], out1[19], out1[25], out1[31], out1[37], out1[43], out1[49],
  out2[31], out2[37], out2[43], out2[49]
};

Mesh.ElementOrder = 1; Mesh.MshFileVersion = 2.2; Mesh 3;
    )")
        .arg(radius).arg(wall).arg(h_base).arg(h_wall)
        .arg(nC).arg(nR_in).arg(nR_wall).arg(nH_base).arg(nH_wall)
        .arg(cx).arg(cy).arg(cz); // 传入坐标参数 
}