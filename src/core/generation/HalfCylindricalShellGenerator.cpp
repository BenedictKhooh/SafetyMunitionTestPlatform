#include "HalfCylindricalShellGenerator.h"
#include <QtMath>

QString HalfCylindricalShellGenerator::buildGeoScript(double radius, double height, double lid, double wall, double meshSize, double cx, double cy, double cz) const {

    int nC = qMax(2, (int)qRound((radius / 1.41421356) / meshSize)) + 1;
    int nR_wall = qMax(1, (int)qRound(wall / meshSize)) + 1;
    int nH_cap = qMax(1, (int)qRound(lid / meshSize));
    double voidHeight = height - 2.0 * lid;
    int nH_void = qMax(1, (int)qRound(voidHeight / meshSize));

    return QString(R"(
// 参数定义
R_in = %1; Wall = %2; H_height = %3; H_cap = %4;
R_out = R_in + Wall;
H_void = H_height - 2 * H_cap;

nC = %5; nR_wall = %6; nH_cap = %7; nH_void = %8;
CX = %9; CY = %10; CZ = %11;

L_in = R_in / (2 * 1.41421356);
p_in = R_in / 1.41421356;
p_out = R_out / 1.41421356;

// 点定义
Point(1) = {CX + 0, CY + 0, CZ + 0};
Point(2) = {CX - L_in, CY + 0, CZ + 0}; Point(3) = {CX + L_in, CY + 0, CZ + 0};
Point(4) = {CX + L_in, CY + L_in, CZ + 0}; Point(5) = {CX - L_in, CY + L_in, CZ + 0};
Point(6) = {CX - R_in, CY + 0, CZ + 0}; Point(7) = {CX + R_in, CY + 0, CZ + 0};
Point(8) = {CX + p_in, CY + p_in, CZ + 0}; Point(9) = {CX - p_in, CY + p_in, CZ + 0};
Point(10) = {CX - R_out, CY + 0, CZ + 0}; Point(11) = {CX + R_out, CY + 0, CZ + 0};
Point(12) = {CX + p_out, CY + p_out, CZ + 0}; Point(13) = {CX - p_out, CY + p_out, CZ + 0};

// 线定义
Line(1)={2,3}; Line(2)={3,4}; Line(3)={4,5}; Line(4)={5,2};
Line(5)={6,2}; Line(6)={3,7}; Line(7)={4,8}; Line(8)={5,9};
Circle(9)={7,1,8}; Circle(10)={8,1,9}; Circle(11)={9,1,6};
Line(12)={7,11}; Line(13)={8,12}; Line(14)={9,13}; Line(15)={6,10};
Circle(16)={11,1,12}; Circle(17)={12,1,13}; Circle(18)={13,1,10};

// 面定义
Curve Loop(101) = {1, 2, 3, 4};        Plane Surface(1) = {101};
Curve Loop(102) = {6, 9, -7, -2};      Plane Surface(2) = {102};
Curve Loop(103) = {7, 10, -8, -3};     Plane Surface(3) = {103};
Curve Loop(104) = {8, 11, 5, -4};      Plane Surface(4) = {104};
Curve Loop(105) = {12, 16, -13, -9};   Plane Surface(5) = {105};
Curve Loop(106) = {13, 17, -14, -10};  Plane Surface(6) = {106};
Curve Loop(107) = {14, 18, -15, -11};  Plane Surface(7) = {107};

// 结构化约束
Transfinite Curve {1, 3, 10, 17} = nC;
Transfinite Curve {2, 4, 9, 11, 16, 18} = nC; 
Transfinite Curve {5, 6, 7, 8} = nC; 
Transfinite Curve {12, 13, 14, 15} = nR_wall;

Transfinite Surface {1:7}; 
Recombine Surface {1:7};

// 分步拉伸三层：底盖、中间壳、顶盖
out1_1[] = Extrude {0, 0, H_cap} { Surface{1}; Layers{nH_cap}; Recombine; };
out1_2[] = Extrude {0, 0, H_cap} { Surface{2}; Layers{nH_cap}; Recombine; };
out1_3[] = Extrude {0, 0, H_cap} { Surface{3}; Layers{nH_cap}; Recombine; };
out1_4[] = Extrude {0, 0, H_cap} { Surface{4}; Layers{nH_cap}; Recombine; };
out1_5[] = Extrude {0, 0, H_cap} { Surface{5}; Layers{nH_cap}; Recombine; };
out1_6[] = Extrude {0, 0, H_cap} { Surface{6}; Layers{nH_cap}; Recombine; };
out1_7[] = Extrude {0, 0, H_cap} { Surface{7}; Layers{nH_cap}; Recombine; };

out2_1[] = Extrude {0, 0, H_void} { Surface{out1_1[0]}; Layers{nH_void}; Recombine; };
out2_2[] = Extrude {0, 0, H_void} { Surface{out1_2[0]}; Layers{nH_void}; Recombine; };
out2_3[] = Extrude {0, 0, H_void} { Surface{out1_3[0]}; Layers{nH_void}; Recombine; };
out2_4[] = Extrude {0, 0, H_void} { Surface{out1_4[0]}; Layers{nH_void}; Recombine; };
out2_5[] = Extrude {0, 0, H_void} { Surface{out1_5[0]}; Layers{nH_void}; Recombine; };
out2_6[] = Extrude {0, 0, H_void} { Surface{out1_6[0]}; Layers{nH_void}; Recombine; };
out2_7[] = Extrude {0, 0, H_void} { Surface{out1_7[0]}; Layers{nH_void}; Recombine; };

out3_1[] = Extrude {0, 0, H_cap} { Surface{out2_1[0]}; Layers{nH_cap}; Recombine; };
out3_2[] = Extrude {0, 0, H_cap} { Surface{out2_2[0]}; Layers{nH_cap}; Recombine; };
out3_3[] = Extrude {0, 0, H_cap} { Surface{out2_3[0]}; Layers{nH_cap}; Recombine; };
out3_4[] = Extrude {0, 0, H_cap} { Surface{out2_4[0]}; Layers{nH_cap}; Recombine; };
out3_5[] = Extrude {0, 0, H_cap} { Surface{out2_5[0]}; Layers{nH_cap}; Recombine; };
out3_6[] = Extrude {0, 0, H_cap} { Surface{out2_6[0]}; Layers{nH_cap}; Recombine; };
out3_7[] = Extrude {0, 0, H_cap} { Surface{out2_7[0]}; Layers{nH_cap}; Recombine; };

// 挖空装药区域
Delete { Volume{out2_1[1], out2_2[1], out2_3[1], out2_4[1]}; }

// 🌟 物理粘合剂：消除各层独立拉伸造成的重叠边界，将其变成完美一体的网格！
Coherence;

// 🌟 动态分配剩余的所有实体到物理组
Physical Volume("Half_Shell_Solid") = Volume "*";

// 🌟 强制兼容性输出
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; 
Mesh 3;
)")
.arg(radius).arg(wall).arg(height).arg(lid)
.arg(nC).arg(nR_wall).arg(nH_cap).arg(nH_void)
.arg(cx).arg(cy).arg(cz);
}