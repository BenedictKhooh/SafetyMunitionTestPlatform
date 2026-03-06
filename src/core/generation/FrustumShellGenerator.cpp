#include "FrustumShellGenerator.h"

QString FrustumShellGenerator::buildGeoScript(double rInBot, double rInTop, double wall, double hCap, double hVoid,
    int nC, int nRIn, int nRWall, int nHCap, int nHVoid) const {
    return QString(R"(
// ==========================================
// 1. 基础参数定义 (由 C++ 注入)
// ==========================================
R_in_bot = %1; R_in_top = %2; Wall = %3;
H_cap = %4; H_void = %5;

nC = %6; nR_in = %7; nR_wall = %8; nH_cap = %9; nH_void = %10;

H_total = 2 * H_cap + H_void;
Z_arr[0] = 0; Z_arr[1] = H_cap; Z_arr[2] = H_cap + H_void; Z_arr[3] = H_total;
nLayers_arr[0] = nH_cap; nLayers_arr[1] = nH_void; nLayers_arr[2] = nH_cap;

For K In {0:3}
    R_in_arr[K] = R_in_bot + (R_in_top - R_in_bot) * (Z_arr[K] / H_total);
    R_out_arr[K] = R_in_arr[K] + Wall;
EndFor

// ==========================================
// 2. 宏定义：生成点、线、面
// ==========================================
Macro BuildPointsAndLines
    p0 = 100 * K; z = Z_arr[K]; ri = R_in_arr[K]; ro = R_out_arr[K];
    lv = ri / (2 * 1.414); pi = ri / 1.414; po = ro / 1.414;

    Point(p0+1)={0,0,z}; Point(p0+2)={lv,lv,z}; Point(p0+3)={-lv,lv,z};
    Point(p0+4)={-lv,-lv,z}; Point(p0+5)={lv,-lv,z}; Point(p0+6)={pi,pi,z};
    Point(p0+7)={-pi,pi,z}; Point(p0+8)={-pi,-pi,z}; Point(p0+9)={pi,-pi,z};
    Point(p0+10)={po,po,z}; Point(p0+11)={-po,po,z}; Point(p0+12)={-po,-po,z}; Point(p0+13)={po,-po,z};

    lb = 100 * K; c = p0+1;
    Line(lb+1)={p0+2,p0+3}; Line(lb+2)={p0+3,p0+4}; Line(lb+3)={p0+4,p0+5}; Line(lb+4)={p0+5,p0+2};
    Line(lb+5)={p0+2,p0+6}; Line(lb+6)={p0+3,p0+7}; Line(lb+7)={p0+4,p0+8}; Line(lb+8)={p0+5,p0+9};
    Circle(lb+9)={p0+6,c,p0+7}; Circle(lb+10)={p0+7,c,p0+8}; Circle(lb+11)={p0+8,c,p0+9}; Circle(lb+12)={p0+9,c,p0+6};
    Line(lb+13)={p0+6,p0+10}; Line(lb+14)={p0+7,p0+11}; Line(lb+15)={p0+8,p0+12}; Line(lb+16)={p0+9,p0+13};
    Circle(lb+17)={p0+10,c,p0+11}; Circle(lb+18)={p0+11,c,p0+12}; Circle(lb+19)={p0+12,c,p0+13}; Circle(lb+20)={p0+13,c,p0+10};

    Transfinite Curve {lb+1:lb+4, lb+9:lb+12, lb+17:lb+20} = nC;
    Transfinite Curve {lb+5:lb+8} = nR_in;
    Transfinite Curve {lb+13:lb+16} = nR_wall;
Return

Macro BuildHorizontalSurfaces
    lb = 100 * K; sh = 3000 + 100 * K;
    Curve Loop(sh+1)={lb+1,lb+2,lb+3,lb+4}; Plane Surface(sh+1)={sh+1};
    Curve Loop(sh+2)={lb+5,lb+9,-(lb+6),-(lb+1)}; Plane Surface(sh+2)={sh+2};
    Curve Loop(sh+3)={lb+6,lb+10,-(lb+7),-(lb+2)}; Plane Surface(sh+3)={sh+3};
    Curve Loop(sh+4)={lb+7,lb+11,-(lb+8),-(lb+3)}; Plane Surface(sh+4)={sh+4};
    Curve Loop(sh+5)={lb+8,lb+12,-(lb+5),-(lb+4)}; Plane Surface(sh+5)={sh+5};
    Curve Loop(sh+6)={lb+13,lb+17,-(lb+14),-(lb+9)}; Plane Surface(sh+6)={sh+6};
    Curve Loop(sh+7)={lb+14,lb+18,-(lb+15),-(lb+10)}; Plane Surface(sh+7)={sh+7};
    Curve Loop(sh+8)={lb+15,lb+19,-(lb+16),-(lb+11)}; Plane Surface(sh+8)={sh+8};
    Curve Loop(sh+9)={lb+16,lb+20,-(lb+13),-(lb+12)}; Plane Surface(sh+9)={sh+9};
    Transfinite Surface {sh+1:sh+9}; Recombine Surface {sh+1:sh+9};
Return

// ==========================================
// 3. 宏定义：生成体
// ==========================================
Macro BuildVerticalLayerAndVolumes
    lb = 100 * L; lt = 100 * (L+1); lv = 1000 + 100 * L; sv = 2000 + 100 * L;
    p0 = 100 * L; p1 = 100 * (L+1); nl = nLayers_arr[L];
    sb = 3000 + 100 * L; st = 3000 + 100 * (L+1); vol = 4000 + 100 * L;

    For i In {6:13}
        Line(lv+i)={p0+i, p1+i}; Transfinite Curve {lv+i}=nl;
    EndFor
    Curve Loop(sv+9)={lb+9,lv+7,-(lt+9),-(lv+6)}; Surface(sv+9)={sv+9};
    Curve Loop(sv+10)={lb+10,lv+8,-(lt+10),-(lv+7)}; Surface(sv+10)={sv+10};
    Curve Loop(sv+11)={lb+11,lv+9,-(lt+11),-(lv+8)}; Surface(sv+11)={sv+11};
    Curve Loop(sv+12)={lb+12,lv+6,-(lt+12),-(lv+9)}; Surface(sv+12)={sv+12};
    Curve Loop(sv+13)={lb+13,lv+10,-(lt+13),-(lv+6)}; Plane Surface(sv+13)={sv+13};
    Curve Loop(sv+14)={lb+14,lv+11,-(lt+14),-(lv+7)}; Plane Surface(sv+14)={sv+14};
    Curve Loop(sv+15)={lb+15,lv+12,-(lt+15),-(lv+8)}; Plane Surface(sv+15)={sv+15};
    Curve Loop(sv+16)={lb+16,lv+13,-(lt+16),-(lv+9)}; Plane Surface(sv+16)={sv+16};
    Curve Loop(sv+17)={lb+17,lv+11,-(lt+17),-(lv+10)}; Surface(sv+17)={sv+17};
    Curve Loop(sv+18)={lb+18,lv+12,-(lt+18),-(lv+11)}; Surface(sv+18)={sv+18};
    Curve Loop(sv+19)={lb+19,lv+13,-(lt+19),-(lv+12)}; Surface(sv+19)={sv+19};
    Curve Loop(sv+20)={lb+20,lv+10,-(lt+20),-(lv+13)}; Surface(sv+20)={sv+20};
    Transfinite Surface {sv+9:sv+20}; Recombine Surface {sv+9:sv+20};
    Surface Loop(vol+6)={sb+6, st+6, sv+13, sv+17, -(sv+14), -(sv+9)}; Volume(vol+6)={vol+6};
    Surface Loop(vol+7)={sb+7, st+7, sv+14, sv+18, -(sv+15), -(sv+10)}; Volume(vol+7)={vol+7};
    Surface Loop(vol+8)={sb+8, st+8, sv+15, sv+19, -(sv+16), -(sv+11)}; Volume(vol+8)={vol+8};
    Surface Loop(vol+9)={sb+9, st+9, sv+16, sv+20, -(sv+13), -(sv+12)}; Volume(vol+9)={vol+9};
    Transfinite Volume {vol+6:vol+9}; Recombine Volume {vol+6:vol+9};

    If (BuildInner == 1)
        For i In {2:5}
            Line(lv+i)={p0+i, p1+i}; Transfinite Curve {lv+i}=nl;
        EndFor
        Curve Loop(sv+1)={lb+1,lv+3,-(lt+1),-(lv+2)}; Plane Surface(sv+1)={sv+1};
        Curve Loop(sv+2)={lb+2,lv+4,-(lt+2),-(lv+3)}; Plane Surface(sv+2)={sv+2};
        Curve Loop(sv+3)={lb+3,lv+5,-(lt+3),-(lv+4)}; Plane Surface(sv+3)={sv+3};
        Curve Loop(sv+4)={lb+4,lv+2,-(lt+4),-(lv+5)}; Plane Surface(sv+4)={sv+4};
        Curve Loop(sv+5)={lb+5,lv+6,-(lt+5),-(lv+2)}; Plane Surface(sv+5)={sv+5};
        Curve Loop(sv+6)={lb+6,lv+7,-(lt+6),-(lv+3)}; Plane Surface(sv+6)={sv+6};
        Curve Loop(sv+7)={lb+7,lv+8,-(lt+7),-(lv+4)}; Plane Surface(sv+7)={sv+7};
        Curve Loop(sv+8)={lb+8,lv+9,-(lt+8),-(lv+5)}; Plane Surface(sv+8)={sv+8};
        Transfinite Surface {sv+1:sv+8}; Recombine Surface {sv+1:sv+8};
        Surface Loop(vol+1)={sb+1, st+1, sv+1, sv+2, sv+3, sv+4}; Volume(vol+1)={vol+1};
        Surface Loop(vol+2)={sb+2, st+2, sv+5, sv+9, -(sv+6), -(sv+1)}; Volume(vol+2)={vol+2};
        Surface Loop(vol+3)={sb+3, st+3, sv+6, sv+10, -(sv+7), -(sv+2)}; Volume(vol+3)={vol+3};
        Surface Loop(vol+4)={sb+4, st+4, sv+7, sv+11, -(sv+8), -(sv+3)}; Volume(vol+4)={vol+4};
        Surface Loop(vol+5)={sb+5, st+5, sv+8, sv+12, -(sv+5), -(sv+4)}; Volume(vol+5)={vol+5};
        Transfinite Volume {vol+1:vol+5}; Recombine Volume {vol+1:vol+5};
    EndIf
Return

// ==========================================
// 4. 执行逻辑
// ==========================================
For K In {0:3}
    Call BuildPointsAndLines; Call BuildHorizontalSurfaces;
EndFor

L=0; BuildInner=1; Call BuildVerticalLayerAndVolumes; // 底盖
L=1; BuildInner=0; Call BuildVerticalLayerAndVolumes; // 空腔壳体
L=2; BuildInner=1; Call BuildVerticalLayerAndVolumes; // 顶盖

// 物理组输出
Physical Volume("Bottom_Lid")  = {4001:4009};
Physical Volume("Middle_Shell") = {4106:4109};
Physical Volume("Top_Lid")     = {4201:4209};

Mesh.RecombineAll = 1;
Mesh 3;
Mesh.MshFileVersion = 2.2;
)")
.arg(rInBot).arg(rInTop).arg(wall)
.arg(hCap).arg(hVoid)
.arg(nC).arg(nRIn).arg(nRWall).arg(nHCap).arg(nHVoid);
}