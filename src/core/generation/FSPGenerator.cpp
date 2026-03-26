#include "FSPGenerator.h"
#include <QtMath>

QString FSPGenerator::buildGeoScript(double r, double hb, double hn, double rt, double ms,
    double cx, double cy, double cz) const {
    // 保护机制：防止完全尖锥 (Rt=0) 导致底层计算报除零或拓扑奇点错误
    if (rt < 0.001) rt = 0.001;

    return QString(R"(
R = %1;
Hb = %2;
Hn = %3;
Rt = %4;
ms = %5;

nC = Max(2, Round((R * 0.70710678) / ms)) + 1;
nR = Max(2, Round((R - R * 0.4) / ms)) + 1;
nHb = Max(1, Round(Hb / ms)); 
nHn = Max(1, Round(Hn / ms));

CX = %6; CY = %7; CZ = %8;

// 第一阶段：生成底、中、顶三层 O-Grid 拓扑截面
For i In {0:2}
  If (i == 0)
    o = 0; rad = R; zh = 0;
  EndIf
  If (i == 1)
    o = 20; rad = R; zh = Hb;
  EndIf
  If (i == 2)
    o = 40; rad = Rt; zh = Hb+Hn;
  EndIf
  
  L = rad * 0.4; Rp = rad * 0.70710678;
  
  Point(o+1) = {CX, CY, CZ+zh};
  Point(o+2) = {CX+L, CY+L, CZ+zh};     Point(o+3) = {CX-L, CY+L, CZ+zh};
  Point(o+4) = {CX-L, CY-L, CZ+zh};     Point(o+5) = {CX+L, CY-L, CZ+zh};
  Point(o+6) = {CX+Rp, CY+Rp, CZ+zh};   Point(o+7) = {CX-Rp, CY+Rp, CZ+zh};
  Point(o+8) = {CX-Rp, CY-Rp, CZ+zh};   Point(o+9) = {CX+Rp, CY-Rp, CZ+zh};
  
  Line(o+1)={o+2,o+3}; Line(o+2)={o+3,o+4}; Line(o+3)={o+4,o+5}; Line(o+4)={o+5,o+2};
  Line(o+5)={o+2,o+6}; Line(o+6)={o+3,o+7}; Line(o+7)={o+4,o+8}; Line(o+8)={o+5,o+9};
  Circle(o+9)={o+6,o+1,o+7}; Circle(o+10)={o+7,o+1,o+8};
  Circle(o+11)={o+8,o+1,o+9}; Circle(o+12)={o+9,o+1,o+6};
  
  Curve Loop(o+1)={o+1,o+2,o+3,o+4};             Plane Surface(o+1)={o+1};
  Curve Loop(o+2)={o+1,o+6,-(o+9),-(o+5)};       Plane Surface(o+2)={o+2};
  Curve Loop(o+3)={o+2,o+7,-(o+10),-(o+6)};      Plane Surface(o+3)={o+3};
  Curve Loop(o+4)={o+3,o+8,-(o+11),-(o+7)};      Plane Surface(o+4)={o+4};
  Curve Loop(o+5)={o+4,o+5,-(o+12),-(o+8)};      Plane Surface(o+5)={o+5};
  
  Transfinite Curve {o+1, o+3, o+9, o+11} = nC;
  Transfinite Curve {o+2, o+4, o+10, o+12} = nC;
  Transfinite Curve {o+5, o+6, o+7, o+8} = nR;
  Transfinite Surface {o+1:o+5};
  Recombine Surface {o+1:o+5};
EndFor

// 第二阶段：将 3 层截面连接成 10 个 3D 六面体块
For v In {0:1}
  If (v == 0)
    o1 = 0; o2 = 20; lo = 100; vo = 1000; side_o = 100; nH_sec = nHb + 1;
  EndIf
  If (v == 1)
    o1 = 20; o2 = 40; lo = 200; vo = 2000; side_o = 200; nH_sec = nHn + 1;
  EndIf
  
  Line(lo+1)={o1+1,o2+1};
  Line(lo+2)={o1+2,o2+2}; Line(lo+3)={o1+3,o2+3};
  Line(lo+4)={o1+4,o2+4}; Line(lo+5)={o1+5,o2+5};
  Line(lo+6)={o1+6,o2+6}; Line(lo+7)={o1+7,o2+7};
  Line(lo+8)={o1+8,o2+8}; Line(lo+9)={o1+9,o2+9};
  
  Curve Loop(side_o+1) = {o1+1, lo+3, -(o2+1), -(lo+2)}; Surface(side_o+1) = {side_o+1};
  Curve Loop(side_o+2) = {o1+2, lo+4, -(o2+2), -(lo+3)}; Surface(side_o+2) = {side_o+2};
  Curve Loop(side_o+3) = {o1+3, lo+5, -(o2+3), -(lo+4)}; Surface(side_o+3) = {side_o+3};
  Curve Loop(side_o+4) = {o1+4, lo+2, -(o2+4), -(lo+5)}; Surface(side_o+4) = {side_o+4};
  
  Curve Loop(side_o+5) = {o1+9, lo+7, -(o2+9), -(lo+6)}; Surface(side_o+5) = {side_o+5};
  Curve Loop(side_o+6) = {o1+5, lo+6, -(o2+5), -(lo+2)}; Surface(side_o+6) = {side_o+6};
  Curve Loop(side_o+7) = {o1+6, lo+7, -(o2+6), -(lo+3)}; Surface(side_o+7) = {side_o+7};
  
  Curve Loop(side_o+8) = {o1+10, lo+8, -(o2+10), -(lo+7)}; Surface(side_o+8) = {side_o+8};
  Curve Loop(side_o+9) = {o1+7, lo+8, -(o2+7), -(lo+4)}; Surface(side_o+9) = {side_o+9};
  
  Curve Loop(side_o+10) = {o1+11, lo+9, -(o2+11), -(lo+8)}; Surface(side_o+10) = {side_o+10};
  Curve Loop(side_o+11) = {o1+8, lo+9, -(o2+8), -(lo+5)}; Surface(side_o+11) = {side_o+11};
  
  Curve Loop(side_o+12) = {o1+12, lo+6, -(o2+12), -(lo+9)}; Surface(side_o+12) = {side_o+12};
  
  Surface Loop(vo+1) = {o1+1, o2+1, side_o+1, side_o+2, side_o+3, side_o+4}; Volume(vo+1) = {vo+1};
  Surface Loop(vo+2) = {o1+2, o2+2, side_o+5, side_o+6, side_o+7, side_o+1}; Volume(vo+2) = {vo+2};
  Surface Loop(vo+3) = {o1+3, o2+3, side_o+8, side_o+9, side_o+7, side_o+2}; Volume(vo+3) = {vo+3};
  Surface Loop(vo+4) = {o1+4, o2+4, side_o+10, side_o+11, side_o+9, side_o+3}; Volume(vo+4) = {vo+4};
  Surface Loop(vo+5) = {o1+5, o2+5, side_o+12, side_o+6, side_o+11, side_o+4}; Volume(vo+5) = {vo+5};
  
  Transfinite Curve {lo+1:lo+9} = nH_sec;
  Transfinite Surface {side_o+1:side_o+12};
  Recombine Surface {side_o+1:side_o+12};
  Transfinite Volume {vo+1:vo+5};
EndFor

Physical Volume("Fragment_Solid") = {1001:1005, 2001:2005};
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; 
Mesh 3;
    )").arg(r).arg(hb).arg(hn).arg(rt).arg(ms).arg(cx).arg(cy).arg(cz);
}