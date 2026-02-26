#include "SphereGenerator.h"

QString SphereGenerator::buildGeoScript(double radius, double meshSize, double cx, double cy, double cz) const {
    // 所有的计算逻辑都在脚本内部由 Gmsh 完成，确保高精度
    return QString(R"(
// --- 局部参数由 C++ 传入 ---
R = %1;           
meshSize = %2;    
CX = %3; CY = %4; CZ = %5; // 传入球心坐标

// 脚本内计算分段（nR: 径向, nC: 核心/周向）
_nC = Round(R * 1.57 / meshSize);
_nR = Round(R / meshSize);
nC = (_nC > 2) ? _nC : 2;
nR = (_nR > 2) ? _nR : 2;

L = R * 0.45;  
P = R / Sqrt(3); 

// --- 点定义 ---
Point(100) = {CX, CY, CZ}; 
Point(1) = { CX+L, CY+L, CZ+L}; Point(2) = {CX-L, CY+L, CZ+L}; 
Point(3) = {CX-L, CY-L, CZ+L}; Point(4) = { CX+L, CY-L, CZ+L};
Point(5) = { CX+L, CY+L, CZ-L}; Point(6) = {CX-L, CY+L, CZ-L}; 
Point(7) = {CX-L, CY-L, CZ-L}; Point(8) = { CX+L, CY-L, CZ-L};

Point(11) = { CX+P, CY+P, CZ+P}; Point(12) = {CX-P, CY+P, CZ+P}; 
Point(13) = {CX-P, CY-P, CZ+P}; Point(14) = { CX+P, CY-P, CZ+P};
Point(15) = { CX+P, CY+P, CZ-P}; Point(16) = {CX-P, CY+P, CZ-P}; 
Point(17) = {CX-P, CY-P, CZ-P}; Point(18) = { CX+P, CY-P, CZ-P};

// --- 线段定义 ---
Line(1)={1,2}; Line(2)={2,3}; Line(3)={3,4}; Line(4)={4,1};
Line(5)={5,6}; Line(6)={6,7}; Line(7)={7,8}; Line(8)={8,5};
Line(9)={1,5}; Line(10)={2,6}; Line(11)={3,7}; Line(12)={4,8};
Circle(21)={11,100,12}; Circle(22)={12,100,13}; Circle(23)={13,100,14}; Circle(24)={14,100,11};
Circle(25)={15,100,16}; Circle(26)={16,100,17}; Circle(27)={17,100,18}; Circle(28)={18,100,15};
Circle(29)={11,100,15}; Circle(30)={12,100,16}; Circle(31)={13,100,17}; Circle(32)={14,100,18};
Line(41)={1,11}; Line(42)={2,12}; Line(43)={3,13}; Line(44)={4,14};
Line(45)={5,15}; Line(46)={6,16}; Line(47)={7,17}; Line(48)={8,18};

// --- 表面与体积定义 ---
Curve Loop(101)={1,2,3,4};     Plane Surface(101)={101}; 
Curve Loop(102)={5,6,7,8};     Plane Surface(102)={102}; 
Curve Loop(103)={1,10,-5,-9};  Plane Surface(103)={103}; 
Curve Loop(104)={2,11,-6,-10}; Plane Surface(104)={104}; 
Curve Loop(105)={3,12,-7,-11}; Plane Surface(105)={105}; 
Curve Loop(106)={4,9,-8,-12};  Plane Surface(106)={106}; 
Curve Loop(201)={21,22,23,24}; Surface(201)={201}; 
Curve Loop(202)={25,26,27,28}; Surface(202)={202}; 
Curve Loop(203)={21,30,-25,-29}; Surface(203)={203}; 
Curve Loop(204)={22,31,-26,-30}; Surface(204)={204}; 
Curve Loop(205)={23,32,-27,-31}; Surface(205)={205}; 
Curve Loop(206)={24,29,-28,-32}; Surface(206)={206}; 
Curve Loop(301)={41,21,-42,-1}; Surface(301)={301}; Curve Loop(302)={42,22,-43,-2}; Surface(302)={302};
Curve Loop(303)={43,23,-44,-3}; Surface(303)={303}; Curve Loop(304)={44,24,-41,-4}; Surface(304)={304};
Curve Loop(305)={45,25,-46,-5}; Surface(305)={305}; Curve Loop(306)={46,26,-47,-6}; Surface(306)={306};
Curve Loop(307)={47,27,-48,-7}; Surface(307)={307}; Curve Loop(308)={48,28,-45,-8}; Surface(308)={308};
Curve Loop(309)={41,29,-45,-9}; Surface(309)={309}; Curve Loop(310)={42,30,-46,-10}; Surface(310)={310};
Curve Loop(311)={43,31,-47,-11}; Surface(311)={311}; Curve Loop(312)={44,32,-48,-12}; Surface(312)={312};

Surface Loop(1) = {101, 102, 103, 104, 105, 106}; Volume(1) = {1}; 
Surface Loop(2) = {101, 201, 301, 302, 303, 304}; Volume(2) = {2}; 
Surface Loop(3) = {102, 202, 305, 306, 307, 308}; Volume(3) = {3}; 
Surface Loop(4) = {103, 203, 301, 305, 309, 310}; Volume(4) = {4}; 
Surface Loop(5) = {104, 204, 302, 306, 310, 311}; Volume(5) = {5}; 
Surface Loop(6) = {105, 205, 303, 307, 311, 312}; Volume(6) = {6}; 
Surface Loop(7) = {106, 206, 304, 308, 312, 309}; Volume(7) = {7}; 

// --- 结构化约束 ---
Transfinite Curve {1:12, 21:32} = nC;
Transfinite Curve {41:48} = nR;
Transfinite Surface "*";
Transfinite Volume "*";
Mesh.RecombineAll = 1;

// --- 强制纯 Type 5 六面体输出 ---
Physical Volume("Sphere_Hex_Mesh") = {1:7};
Mesh.SaveAll = 0; 
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )").arg(radius).arg(meshSize).arg(cx).arg(cy).arg(cz);
}
