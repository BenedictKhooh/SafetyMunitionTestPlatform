#include "RefinedHalfCylindricalShellGenerator.h"

QString RefinedHalfCylindricalShellGenerator::generateGeoScript() const {
    QString script = R"(
SetFactory("Built-in");

R_in   = %1;
H_tot  = %2;
H_cap  = %3;
Wall   = %4;
ms     = %5;
CX = %6; CY = %7; CZ = %8;
Z_start = %9; Z_end = %10; ms_local_z = %11;
ms_wall = %12; // ★ 新增：壁厚局部网格尺寸

R_out  = R_in + Wall;
H_void = H_tot - 2 * H_cap;

If (ms_local_z <= 1e-5)
    ms_local_z = ms;
EndIf

Z_start = (Z_start < 0) ? 0 : Z_start;
Z_end   = (Z_end > H_tot) ? H_tot : Z_end;
Z_start = (Z_start > Z_end) ? Z_end : Z_start;

Z_pts[] = {0, H_cap, H_cap + H_void, H_tot, Z_start, Z_end};
For i In {0 : 4}
    For j In {0 : 4 - i}
        If (Z_pts[j] > Z_pts[j+1])
            tmp = Z_pts[j]; Z_pts[j] = Z_pts[j+1]; Z_pts[j+1] = tmp;
        EndIf
    EndFor
EndFor

Z_unique[] = {Z_pts[0]};
idx = 0;
For i In {1 : 5}
    If (Z_pts[i] - Z_unique[idx] > 1e-5)
        idx++; Z_unique[idx] = Z_pts[i];
    EndIf
EndFor

Rproj_in  = R_in / 1.41421356; 
L_val     = Rproj_in / 2.0; 
Rproj_out = R_out / 1.41421356;

nC_nodes = Max(2, Round(Rproj_in / ms)) + 1;
nL_nodes = Max(2, Round(L_val / ms)) + 1;
nR_in_nodes = Max(2, Round((R_in - L_val) / ms)) + 1;

// ★ 核心修改：使用 ms_wall 计算壁厚的分段节点数
nR_wall_nodes = Max(2, Round(Wall / ms_wall)) + 1;

For k In {0 : idx}
    P  = 10000 + k * 1000;
    HC = 20000 + k * 1000;
    HS = 30000 + k * 1000;
    z  = Z_unique[k];

    Point(P + 1) = {CX + 0, CY + 0, CZ + z};
    Point(P + 2) = {CX - L_val, CY + 0, CZ + z};
    Point(P + 3) = {CX + L_val, CY + 0, CZ + z};
    Point(P + 4) = {CX + L_val, CY + L_val, CZ + z};
    Point(P + 5) = {CX - L_val, CY + L_val, CZ + z};
    Point(P + 6) = {CX - R_in, CY + 0, CZ + z};
    Point(P + 7) = {CX + R_in, CY + 0, CZ + z};
    Point(P + 8) = {CX + Rproj_in, CY + Rproj_in, CZ + z};
    Point(P + 9) = {CX - Rproj_in, CY + Rproj_in, CZ + z};
    Point(P + 10) = {CX - R_out, CY + 0, CZ + z};
    Point(P + 11) = {CX + R_out, CY + 0, CZ + z};
    Point(P + 12) = {CX + Rproj_out, CY + Rproj_out, CZ + z};
    Point(P + 13) = {CX - Rproj_out, CY + Rproj_out, CZ + z};

    Line(HC + 1) = {P+2, P+3}; Line(HC + 2) = {P+3, P+4}; 
    Line(HC + 3) = {P+4, P+5}; Line(HC + 4) = {P+5, P+2}; 
    Line(HC + 5) = {P+6, P+2}; Line(HC + 6) = {P+3, P+7}; 
    Line(HC + 7) = {P+4, P+8}; Line(HC + 8) = {P+5, P+9}; 
    Circle(HC + 9)  = {P+7, P+1, P+8}; 
    Circle(HC + 10) = {P+8, P+1, P+9}; 
    Circle(HC + 11) = {P+9, P+1, P+6};
    Line(HC + 12) = {P+6, P+10}; Line(HC + 13) = {P+7, P+11}; 
    Line(HC + 14) = {P+8, P+12}; Line(HC + 15) = {P+9, P+13}; 
    Circle(HC + 16) = {P+11, P+1, P+12}; 
    Circle(HC + 17) = {P+12, P+1, P+13}; 
    Circle(HC + 18) = {P+13, P+1, P+10};

    Curve Loop(HS + 1) = {HC+1, HC+2, HC+3, HC+4};         Plane Surface(HS + 1) = {HS+1};
    Curve Loop(HS + 2) = {HC+6, HC+9, -(HC+7), -(HC+2)};   Plane Surface(HS + 2) = {HS+2};
    Curve Loop(HS + 3) = {HC+7, HC+10, -(HC+8), -(HC+3)};  Plane Surface(HS + 3) = {HS+3};
    Curve Loop(HS + 4) = {HC+8, HC+11, HC+5, -(HC+4)};     Plane Surface(HS + 4) = {HS+4};
    Curve Loop(HS + 5) = {HC+13, HC+16, -(HC+14), -(HC+9)};Plane Surface(HS + 5) = {HS+5};
    Curve Loop(HS + 6) = {HC+14, HC+17, -(HC+15), -(HC+10)};Plane Surface(HS + 6) = {HS+6};
    Curve Loop(HS + 7) = {HC+15, HC+18, -(HC+12), -(HC+11)};Plane Surface(HS + 7) = {HS+7};

    Transfinite Curve {HC+2, HC+4, HC+9, HC+11, HC+16, HC+18} = nL_nodes;
    Transfinite Curve {HC+1, HC+3, HC+10, HC+17} = nC_nodes;
    Transfinite Curve {HC+5, HC+6, HC+7, HC+8} = nR_in_nodes;
    Transfinite Curve {HC+12, HC+13, HC+14, HC+15} = nR_wall_nodes;
EndFor

keep_vols[] = {};

For k In {0 : idx-1}
    P0 = 10000 + k * 1000;     P1 = 10000 + (k+1) * 1000;
    HC0 = 20000 + k * 1000;    HC1 = 20000 + (k+1) * 1000;
    HS0 = 30000 + k * 1000;    HS1 = 30000 + (k+1) * 1000;

    VC = 40000 + k * 1000;  VS = 50000 + k * 1000;  VL = 60000 + k * 1000;  
    
    mid_z = (Z_unique[k] + Z_unique[k+1]) / 2.0;
    dz = Z_unique[k+1] - Z_unique[k];

    If (mid_z > Z_start - 1e-4 && mid_z < Z_end + 1e-4)
        nH = Max(1, Round(dz / ms_local_z));
    Else
        nH = Max(1, Round(dz / ms));
    EndIf

    For i In {2:13}
        Line(VC + i) = {P0+i, P1+i};
        Transfinite Curve {VC + i} = nH + 1;
    EndFor

    Curve Loop(VS+1) = {HC0+1, VC+3, -(HC1+1), -(VC+2)}; Surface(VS+1)={VS+1};
    Curve Loop(VS+2) = {HC0+2, VC+4, -(HC1+2), -(VC+3)}; Surface(VS+2)={VS+2};
    Curve Loop(VS+3) = {HC0+3, VC+5, -(HC1+3), -(VC+4)}; Surface(VS+3)={VS+3};
    Curve Loop(VS+4) = {HC0+4, VC+2, -(HC1+4), -(VC+5)}; Surface(VS+4)={VS+4};
    Curve Loop(VS+5) = {HC0+5, VC+2, -(HC1+5), -(VC+6)}; Surface(VS+5)={VS+5};
    Curve Loop(VS+6) = {HC0+6, VC+7, -(HC1+6), -(VC+3)}; Surface(VS+6)={VS+6};
    Curve Loop(VS+7) = {HC0+7, VC+8, -(HC1+7), -(VC+4)}; Surface(VS+7)={VS+7};
    Curve Loop(VS+8) = {HC0+8, VC+9, -(HC1+8), -(VC+5)}; Surface(VS+8)={VS+8};
    Curve Loop(VS+9) = {HC0+9, VC+8, -(HC1+9), -(VC+7)}; Surface(VS+9)={VS+9};
    Curve Loop(VS+10) = {HC0+10, VC+9, -(HC1+10), -(VC+8)}; Surface(VS+10)={VS+10};
    Curve Loop(VS+11) = {HC0+11, VC+6, -(HC1+11), -(VC+9)}; Surface(VS+11)={VS+11};
    Curve Loop(VS+12) = {HC0+12, VC+10, -(HC1+12), -(VC+6)}; Surface(VS+12)={VS+12};
    Curve Loop(VS+13) = {HC0+13, VC+11, -(HC1+13), -(VC+7)}; Surface(VS+13)={VS+13};
    Curve Loop(VS+14) = {HC0+14, VC+12, -(HC1+14), -(VC+8)}; Surface(VS+14)={VS+14};
    Curve Loop(VS+15) = {HC0+15, VC+13, -(HC1+15), -(VC+9)}; Surface(VS+15)={VS+15};
    Curve Loop(VS+16) = {HC0+16, VC+12, -(HC1+16), -(VC+11)}; Surface(VS+16)={VS+16};
    Curve Loop(VS+17) = {HC0+17, VC+13, -(HC1+17), -(VC+12)}; Surface(VS+17)={VS+17};
    Curve Loop(VS+18) = {HC0+18, VC+10, -(HC1+18), -(VC+13)}; Surface(VS+18)={VS+18};

    is_void = (mid_z > H_cap + 1e-4 && mid_z < H_cap + H_void - 1e-4);

    Surface Loop(VL+5) = {HS0+5, HS1+5, VS+13, VS+16, VS+14, VS+9}; Volume(VL+5)={VL+5};
    Surface Loop(VL+6) = {HS0+6, HS1+6, VS+14, VS+17, VS+15, VS+10}; Volume(VL+6)={VL+6};
    Surface Loop(VL+7) = {HS0+7, HS1+7, VS+15, VS+18, VS+12, VS+11}; Volume(VL+7)={VL+7};
    keep_vols[] += {VL+5, VL+6, VL+7};

    If (!is_void)
        Surface Loop(VL+1) = {HS0+1, HS1+1, VS+1, VS+2, VS+3, VS+4}; Volume(VL+1)={VL+1};
        Surface Loop(VL+2) = {HS0+2, HS1+2, VS+6, VS+9, VS+7, VS+2}; Volume(VL+2)={VL+2};
        Surface Loop(VL+3) = {HS0+3, HS1+3, VS+7, VS+10, VS+8, VS+3}; Volume(VL+3)={VL+3};
        Surface Loop(VL+4) = {HS0+4, HS1+4, VS+8, VS+11, VS+5, VS+4}; Volume(VL+4)={VL+4};
        keep_vols[] += {VL+1, VL+2, VL+3, VL+4};
    EndIf
EndFor

Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";
Physical Volume("HalfCylindricalShell_Solid") = {keep_vols[]};

Mesh.RecombineAll = 1;
Mesh.SurfaceEdges = 1;
Mesh.VolumeEdges  = 1;
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0;
Mesh 3;
)";

    return script.arg(m_rIn).arg(m_height).arg(m_lid).arg(m_wall).arg(m_meshSize)
        .arg(m_cx).arg(m_cy).arg(m_cz)
        .arg(m_zStart).arg(m_zEnd).arg(m_msLocalZ).arg(m_msWall);
}