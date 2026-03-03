#include "HemisphereGenerator.h"

QString HemisphereGenerator::buildGeoScript(double radius, double meshSize, double cx, double cy, double cz) const {
    return QString(R"(
// --- 1. 参数设置 (由 C++ 传入) ---
R = %1;           
ms = %2;    
CX = %3; CY = %4; CZ = %5; 

L = 0.45 * R;       

// 动态计算分段数，确保网格密度符合 ms 要求
nR = Max(2, Round(R / ms)); 
nC = Max(2, Round((Pi * R / 2) / ms));

// --- 2. 点定义 (加入坐标偏移) ---
Point(100) = {CX, CY, CZ}; // 原心

// 核心块顶点 (Z=CZ, Z=CZ+L)
Point(1) = { CX+L,  CY+L, CZ};   Point(2) = {CX-L,  CY+L, CZ};
Point(3) = {CX-L,  CY-L, CZ};   Point(4) = { CX+L,  CY-L, CZ};
Point(5) = { CX+L,  CY+L, CZ+L}; Point(6) = {CX-L,  CY+L, CZ+L};
Point(7) = {CX-L,  CY-L, CZ+L}; Point(8) = { CX+L,  CY-L, CZ+L};

// 球面投影顶点
P_base = R / Sqrt(2); 
Point(11) = { CX+P_base,  CY+P_base, CZ}; Point(12) = {CX-P_base,  CY+P_base, CZ};
Point(13) = {CX-P_base,  CY-P_base, CZ}; Point(14) = { CX+P_base,  CY-P_base, CZ};

P = R / Sqrt(3);
Point(15) = { CX+P,  CY+P, CZ+P}; Point(16) = {CX-P,  CY+P, CZ+P};
Point(17) = {CX-P,  CY-P, CZ+P}; Point(18) = { CX+P,  CY-P, CZ+P};

// --- 3. 线段定义 ---
Line(1)={1,2}; Line(2)={2,3}; Line(3)={3,4}; Line(4)={4,1};
Line(5)={5,6}; Line(6)={6,7}; Line(7)={7,8}; Line(8)={8,5};
Line(9)={1,5}; Line(10)={2,6}; Line(11)={3,7}; Line(12)={4,8};

Circle(21)={11,100,12}; Circle(22)={12,100,13}; Circle(23)={13,100,14}; Circle(24)={14,100,11};
Circle(25)={15,100,16}; Circle(26)={16,100,17}; Circle(27)={17,100,18}; Circle(28)={18,100,15};
Circle(29)={11,100,15}; Circle(30)={12,100,16}; Circle(31)={13,100,17}; Circle(32)={14,100,18};

Line(41)={1,11}; Line(42)={2,12}; Line(43)={3,13}; Line(44)={4,14};
Line(45)={5,15}; Line(46)={6,16}; Line(47)={7,17}; Line(48)={8,18};

// --- 4. 表面定义 (Curve Loop -> Surface) ---
Curve Loop(501)={1,2,3,4};     Plane Surface(101)={501}; 
Curve Loop(502)={5,6,7,8};     Plane Surface(102)={502}; 
Curve Loop(503)={1,10,-5,-9};  Plane Surface(103)={503}; 
Curve Loop(504)={2,11,-6,-10}; Plane Surface(104)={504}; 
Curve Loop(505)={3,12,-7,-11}; Plane Surface(105)={505}; 
Curve Loop(506)={4,9,-8,-12};  Plane Surface(106)={506}; 

Curve Loop(507)={25,26,27,28};     Surface(201)={507}; 
Curve Loop(508)={21,30,-25,-29};   Surface(202)={508}; 
Curve Loop(509)={22,31,-26,-30};   Surface(203)={509}; 
Curve Loop(510)={23,32,-27,-31};   Surface(204)={510}; 
Curve Loop(511)={24,29,-28,-32};   Surface(205)={511}; 

Curve Loop(512)={41,21,-42,-1};    Plane Surface(301)={512}; 
Curve Loop(513)={42,22,-43,-2};    Plane Surface(302)={513}; 
Curve Loop(514)={43,23,-44,-3};    Plane Surface(303)={514}; 
Curve Loop(515)={44,24,-41,-4};    Plane Surface(304)={515}; 

Curve Loop(516)={9,45,-29,-41};    Surface(305)={516}; 
Curve Loop(517)={10,46,-30,-42};   Surface(306)={517}; 
Curve Loop(518)={11,47,-31,-43};   Surface(307)={518}; 
Curve Loop(519)={12,48,-32,-44};   Surface(308)={519}; 

Curve Loop(520)={45,25,-46,-5};    Surface(309)={520}; 
Curve Loop(521)={46,26,-47,-6};    Surface(310)={521}; 
Curve Loop(522)={47,27,-48,-7};    Surface(311)={522}; 
Curve Loop(523)={48,28,-45,-8};    Surface(312)={523}; 

// --- 5. 体积定义 ---
Surface Loop(601) = {101, 102, 103, 104, 105, 106}; Volume(1) = {601};
Surface Loop(602) = {102, 201, 309, 310, 311, 312}; Volume(2) = {602};
Surface Loop(603) = {103, 202, 301, 309, 305, 306}; Volume(3) = {603};
Surface Loop(604) = {104, 203, 302, 310, 306, 307}; Volume(4) = {604};
Surface Loop(605) = {105, 204, 303, 311, 307, 308}; Volume(5) = {605};
Surface Loop(606) = {106, 205, 304, 312, 308, 305}; Volume(6) = {606};

// --- 6. 物理组与导出设置 ---
Physical Volume("Hemi_Hex_Mesh") = {1, 2, 3, 4, 5, 6};

Transfinite Curve {1:12, 21:32} = nC;
Transfinite Curve {41:48} = nR;
Transfinite Surface "*";
Transfinite Volume "*";
Mesh.RecombineAll = 1;
Mesh.SaveAll = 0;           // 核心：只导出物理组，过滤掉面网格
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;  // 兼容性版本
Mesh 3;
    )")
        .arg(radius).arg(meshSize)
        .arg(cx).arg(cy).arg(cz);
}