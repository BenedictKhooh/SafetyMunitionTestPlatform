#include "CubeGenerator.h"

QString CubeGenerator::buildGeoScript(double lx, double ly, double lz, double meshSize, double cx, double cy, double cz) const {
    // 计算半长
    double dx = lx / 2.0;
    double dy = ly / 2.0;
    double dz = lz / 2.0;

    return QString(R"(
// --- 局部参数 ---
meshSize = %1;
X0 = %2 - %5; X1 = %2 + %5;
Y0 = %3 - %6; Y1 = %3 + %6;
Z0 = %4 - %7; Z1 = %4 + %7;

// 计算各方向分段数
nx = Max(2, Round(%8 / meshSize));
ny = Max(2, Round(%9 / meshSize));
nz = Max(2, Round(%10 / meshSize));

// --- 点定义 (底面 z0 到 顶面 z1) ---
Point(1) = {X0, Y0, Z0}; Point(2) = {X1, Y0, Z0};
Point(3) = {X1, Y1, Z0}; Point(4) = {X0, Y1, Z0};
Point(5) = {X0, Y0, Z1}; Point(6) = {X1, Y0, Z1};
Point(7) = {X1, Y1, Z1}; Point(8) = {X0, Y1, Z1};

// --- 线段定义 ---
// 底面四条线
Line(1) = {1, 2}; Line(2) = {2, 3}; Line(3) = {3, 4}; Line(4) = {4, 1};
// 顶面四条线
Line(5) = {5, 6}; Line(6) = {6, 7}; Line(7) = {7, 8}; Line(8) = {8, 5};
// 垂直四条线
Line(9) = {1, 5}; Line(10) = {2, 6}; Line(11) = {3, 7}; Line(12) = {4, 8};

// --- 表面定义 ---
Curve Loop(1) = {1, 2, 3, 4};       Plane Surface(1) = {1}; // 底面
Curve Loop(2) = {5, 6, 7, 8};       Plane Surface(2) = {2}; // 顶面
Curve Loop(3) = {1, 10, -5, -9};    Plane Surface(3) = {3}; // 前面
Curve Loop(4) = {2, 11, -6, -10};   Plane Surface(4) = {4}; // 右面
Curve Loop(5) = {3, 12, -7, -11};   Plane Surface(5) = {5}; // 后面
Curve Loop(6) = {4, 9, -8, -12};    Plane Surface(6) = {6}; // 左面

// --- 体积定义 ---
Surface Loop(1) = {1, 2, 3, 4, 5, 6};
Volume(1) = {1};

// --- 结构化约束 (关键：确保纯六面体) ---
Transfinite Curve {1, 3, 5, 7} = nx; // X方向
Transfinite Curve {2, 4, 6, 8} = ny; // Y方向
Transfinite Curve {9, 10, 11, 12} = nz; // Z方向

Transfinite Surface "*";
Transfinite Volume "*";
Mesh.RecombineAll = 1;

// --- 输出设置 ---
Physical Volume("Box_Hex_Mesh") = {1};
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(meshSize)
        .arg(cx).arg(cy).arg(cz) // %2, %3, %4
        .arg(dx).arg(dy).arg(dz) // %5, %6, %7
        .arg(lx).arg(ly).arg(lz); // %8, %9, %10
}