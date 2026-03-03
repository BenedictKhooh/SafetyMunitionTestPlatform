#include "FrustumGenerator.h"

QString FrustumGenerator::buildGeoScript(double rBase, double rTop, double height, double meshSize, double cx, double cy, double cz) const {
    return QString(R"(
// --- 1. 参数设置 ---
RB = %1; RT = %2; H = %3;
ms = %4;
CX = %5; CY = %6; CZ = %7;

ratio = 0.55; 
LB = RB * ratio; LT = RT * ratio;

// 动态分段计算
nC = Max(2, Round((Pi * RB / 2) / ms));
nR = Max(2, Round(RB / ms));
nH = Max(2, Round(H / ms));

// --- 2. 节点定义 (带偏移) ---
Point(100) = {CX, CY, CZ};     // 底圆心
Point(101) = {CX, CY, CZ + H}; // 顶圆心

// 核心块 (1-4 底, 5-8 顶)
Point(1) = { CX+LB, CY+LB, CZ};   Point(2) = {CX-LB, CY+LB, CZ};
Point(3) = {CX-LB, CY-LB, CZ};   Point(4) = { CX+LB, CY-LB, CZ};
Point(5) = { CX+LT, CY+LT, CZ+H}; Point(6) = {CX-LT, CY+LT, CZ+H};
Point(7) = {CX-LT, CY-LT, CZ+H}; Point(8) = { CX+LT, CY-LT, CZ+H};

// 外围投影点
Pb = RB/Sqrt(2); Pt = RT/Sqrt(2);
Point(11) = { CX+Pb, CY+Pb, CZ};   Point(12) = {CX-Pb, CY+Pb, CZ};
Point(13) = {CX-Pb, CY-Pb, CZ};   Point(14) = { CX+Pb, CY-Pb, CZ};
Point(15) = { CX+Pt, CY+Pt, CZ+H}; Point(16) = {CX-Pt, CY+Pt, CZ+H};
Point(17) = {CX-Pt, CY-Pt, CZ+H}; Point(18) = { CX+Pt, CY-Pt, CZ+H};

// --- 3. 线段定义 ---
Line(1)={1,2}; Line(2)={2,3}; Line(3)={3,4}; Line(4)={4,1}; // 底核心
Line(5)={5,6}; Line(6)={6,7}; Line(7)={7,8}; Line(8)={8,5}; // 顶核心
Line(9)={1,5}; Line(10)={2,6}; Line(11)={3,7}; Line(12)={4,8}; // 垂直核心

Circle(21)={11,100,12}; Circle(22)={12,100,13}; Circle(23)={13,100,14}; Circle(24)={14,100,11}; 
Circle(25)={15,101,16}; Circle(26)={16,101,17}; Circle(27)={17,101,18}; Circle(28)={18,101,15}; 

Line(41)={1,11}; Line(42)={2,12}; Line(43)={3,13}; Line(44)={4,14}; // 底放射
Line(45)={5,15}; Line(46)={6,16}; Line(47)={7,17}; Line(48)={8,18}; // 顶放射
Line(51)={11,15}; Line(52)={12,16}; Line(53)={13,17}; Line(54)={14,18}; // 侧缘线

// --- 4. 表面定义 (Loop ID -> Surface ID) ---
Curve Loop(1)={1,2,3,4};     Plane Surface(101)={1};
Curve Loop(2)={5,6,7,8};     Plane Surface(102)={2};
Curve Loop(3)={1,10,-5,-9};  Plane Surface(103)={3};
Curve Loop(4)={2,11,-6,-10}; Plane Surface(104)={4};
Curve Loop(5)={3,12,-7,-11}; Plane Surface(105)={5};
Curve Loop(6)={4,9,-8,-12};  Plane Surface(106)={6};

Curve Loop(7)={41,21,-42,-1};  Plane Surface(107)={7};
Curve Loop(8)={42,22,-43,-2};  Plane Surface(108)={8};
Curve Loop(9)={43,23,-44,-3};  Plane Surface(109)={9};
Curve Loop(10)={44,24,-41,-4}; Plane Surface(110)={10};

Curve Loop(11)={45,25,-46,-5};  Plane Surface(111)={11};
Curve Loop(12)={46,26,-47,-6};  Plane Surface(112)={12};
Curve Loop(13)={47,27,-48,-7};  Plane Surface(113)={13};
Curve Loop(14)={48,28,-45,-8};  Plane Surface(114)={14};

Curve Loop(15)={9,45,-51,-41};  Surface(115)={15};
Curve Loop(16)={10,46,-52,-42}; Surface(116)={16};
Curve Loop(17)={11,47,-53,-43}; Surface(117)={17};
Curve Loop(18)={12,48,-54,-44}; Surface(118)={18};

Curve Loop(19)={21,52,-25,-51}; Surface(119)={19};
Curve Loop(20)={22,53,-26,-52}; Surface(120)={20};
Curve Loop(21)={23,54,-27,-53}; Surface(121)={21};
Curve Loop(22)={24,51,-28,-54}; Surface(122)={22};

// --- 5. 体积定义 ---
Surface Loop(1) = {101:106}; Volume(1) = {1};
Surface Loop(2) = {103, 107, 111, 115, 116, 119}; Volume(2) = {2}; 
Surface Loop(3) = {104, 108, 112, 116, 117, 120}; Volume(3) = {3};
Surface Loop(4) = {105, 109, 113, 117, 118, 121}; Volume(4) = {4};
Surface Loop(5) = {106, 110, 114, 118, 115, 122}; Volume(5) = {5};

// --- 6. 物理组与导出 ---
Physical Volume("Frustum_Hex") = {1:5};

Transfinite Curve {1:12, 21:28, 41:48, 51:54} = nC;
Transfinite Surface "*"; Transfinite Volume "*";
Mesh.RecombineAll = 1;
Mesh.SaveAll = 0;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(rBase).arg(rTop).arg(height).arg(meshSize)
        .arg(cx).arg(cy).arg(cz);
}