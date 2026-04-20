#include "RefinedCylindricalShellGenerator.h"

QString RefinedCylindricalShellGenerator::generateGeoScript() const {
    QString script = R"(
SetFactory("Built-in");

R_in   = %1;
Wall   = %2;
R_out  = R_in + Wall;
H_cap  = %3;
H_void = %4;
H_tot  = 2 * H_cap + H_void;
ms_z   = %5;
CX = %6; CY = %7; CZ = %8;
Z_start = %9;
Z_end   = %10;
ms_local_z = %11;
ms_wall = %12; // ★ 新增：壁厚局部网格尺寸

If (ms_local_z <= 1e-5)
    ms_local_z = ms_z;
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

Z_unique[] = {Z_pts[0]}; idx = 0;
For i In {1 : 5}
    If (Z_pts[i] - Z_unique[idx] > 1e-5)
        idx++; Z_unique[idx] = Z_pts[i];
    EndIf
EndFor

ratio = 0.5; 
arc_len = Pi * R_in / 4.0;
nC = Max(1, Round(arc_len / ms_z));
nR_in = Max(1, Round((R_in - R_in * ratio) / ms_z));

// ★ 核心修改：使用 ms_wall 计算壁厚的分段数
nR_wall = Max(1, Round(Wall / ms_wall));

nC_nodes = nC + 1;
nR_in_nodes = nR_in + 1;
nR_wall_nodes = nR_wall + 1;

For k In {0 : idx}
    P  = 10000 + k * 1000;
    HC = 20000 + k * 1000;
    HS = 30000 + k * 1000;
    z = Z_unique[k]; Lk = R_in * ratio;

    Point(P + 0) = {CX, CY, CZ + z};
    For i In {1:8}
        a = (2*i - 1) * Pi / 8;
        Point(P + i)      = {CX + Lk * Cos(a),   CY + Lk * Sin(a),   CZ + z}; 
        Point(P + 10 + i) = {CX + R_in * Cos(a), CY + R_in * Sin(a), CZ + z}; 
        Point(P + 20 + i) = {CX + R_out * Cos(a),CY + R_out * Sin(a),CZ + z}; 
    EndFor

    Line(HC + 31) = {P, P+1}; Line(HC + 33) = {P, P+3};
    Line(HC + 35) = {P, P+5}; Line(HC + 37) = {P, P+7};
    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Line(HC + 40 + i) = {P+i, P+ni};               
        Line(HC + 50 + i) = {P+i, P+10+i};             
        Circle(HC + 60 + i) = {P+10+i, P, P+10+ni};    
        Line(HC + 70 + i) = {P+10+i, P+20+i};          
        Circle(HC + 80 + i) = {P+20+i, P, P+20+ni};    
    EndFor

    Curve Loop(HS + 101) = {HC+31, HC+41, HC+42, -(HC+33)}; Plane Surface(HS + 101) = {HS+101};
    Curve Loop(HS + 102) = {HC+33, HC+43, HC+44, -(HC+35)}; Plane Surface(HS + 102) = {HS+102};
    Curve Loop(HS + 103) = {HC+35, HC+45, HC+46, -(HC+37)}; Plane Surface(HS + 103) = {HS+103};
    Curve Loop(HS + 104) = {HC+37, HC+47, HC+48, -(HC+31)}; Plane Surface(HS + 104) = {HS+104};
    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(HS + 110 + i) = {HC+50+i, HC+60+i, -(HC+50+ni), -(HC+40+i)}; Plane Surface(HS + 110 + i) = {HS+110+i};
        Curve Loop(HS + 120 + i) = {HC+70+i, HC+80+i, -(HC+70+ni), -(HC+60+i)}; Plane Surface(HS + 120 + i) = {HS+120+i};
    EndFor

    Transfinite Curve {HC+31, HC+33, HC+35, HC+37} = nC_nodes;
    For i In {1:8}
        Transfinite Curve {HC+40+i, HC+60+i, HC+80+i} = nC_nodes;
        Transfinite Curve {HC+50+i} = nR_in_nodes;
        Transfinite Curve {HC+70+i} = nR_wall_nodes;
    EndFor
EndFor

keep_vols[] = {};
For k In {0 : idx-1}
    P0 = 10000 + k * 1000; P1 = 10000 + (k+1) * 1000;
    HC0 = 20000 + k * 1000; HC1 = 20000 + (k+1) * 1000;
    HS0 = 30000 + k * 1000; HS1 = 30000 + (k+1) * 1000;

    VC = 40000 + k * 1000; VS = 50000 + k * 1000; VL = 60000 + k * 1000;  
    
    mid_z = (Z_unique[k] + Z_unique[k+1]) / 2.0;
    dz = Z_unique[k+1] - Z_unique[k];

    If (mid_z > Z_start - 1e-4 && mid_z < Z_end + 1e-4)
        nH = Max(1, Round(dz / ms_local_z));
    Else
        nH = Max(1, Round(dz / ms_z));
    EndIf

    Line(VC + 0) = {P0, P1};
    For i In {1:8}
        Line(VC + i)      = {P0+i, P1+i};
        Line(VC + 10 + i) = {P0+10+i, P1+10+i};
        Line(VC + 20 + i) = {P0+20+i, P1+20+i};
    EndFor

    Transfinite Curve {VC+0} = nH + 1;
    For i In {1:8}
        Transfinite Curve {VC+i, VC+10+i, VC+20+i} = nH + 1;
    EndFor

    Curve Loop(VS + 101) = {HC0+31, VC+1, -(HC1+31), -(VC+0)}; Surface(VS + 101) = {VS+101};
    Curve Loop(VS + 102) = {HC0+33, VC+3, -(HC1+33), -(VC+0)}; Surface(VS + 102) = {VS+102};
    Curve Loop(VS + 103) = {HC0+35, VC+5, -(HC1+35), -(VC+0)}; Surface(VS + 103) = {VS+103};
    Curve Loop(VS + 104) = {HC0+37, VC+7, -(HC1+37), -(VC+0)}; Surface(VS + 104) = {VS+104};

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(VS+110+i) = {HC0+40+i, VC+ni, -(HC1+40+i), -(VC+i)}; Surface(VS+110+i) = {VS+110+i};
        Curve Loop(VS+120+i) = {HC0+50+i, VC+10+i, -(HC1+50+i), -(VC+i)}; Surface(VS+120+i) = {VS+120+i};
        Curve Loop(VS+130+i) = {HC0+60+i, VC+10+ni, -(HC1+60+i), -(VC+10+i)}; Surface(VS+130+i) = {VS+130+i};
        Curve Loop(VS+140+i) = {HC0+70+i, VC+20+i, -(HC1+70+i), -(VC+10+i)}; Surface(VS+140+i) = {VS+140+i};
        Curve Loop(VS+150+i) = {HC0+80+i, VC+20+ni, -(HC1+80+i), -(VC+20+i)}; Surface(VS+150+i) = {VS+150+i};
    EndFor

    is_void = (mid_z > H_cap + 1e-4 && mid_z < H_cap + H_void - 1e-4);

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Surface Loop(VL+220+i) = {HS0+120+i, HS1+120+i, VS+140+i, VS+150+i, VS+140+ni, VS+130+i};
        Volume(VL+220+i) = {VL+220+i};
        keep_vols[] += {VL+220+i};
    EndFor

    If (!is_void)
        Surface Loop(VL+201) = {HS0+101, HS1+101, VS+101, VS+111, VS+112, VS+102}; Volume(VL+201) = {VL+201};
        Surface Loop(VL+202) = {HS0+102, HS1+102, VS+102, VS+113, VS+114, VS+103}; Volume(VL+202) = {VL+202};
        Surface Loop(VL+203) = {HS0+103, HS1+103, VS+103, VS+115, VS+116, VS+104}; Volume(VL+203) = {VL+203};
        Surface Loop(VL+204) = {HS0+104, HS1+104, VS+104, VS+101, VS+117, VS+118}; Volume(VL+204) = {VL+204};
        keep_vols[] += {VL+201, VL+202, VL+203, VL+204};

        For i In {1:8}
            ni = (i==8) ? 1 : i+1;
            Surface Loop(VL+210+i) = {HS0+110+i, HS1+110+i, VS+120+i, VS+130+i, VS+120+ni, VS+110+i};
            Volume(VL+210+i) = {VL+210+i};
            keep_vols[] += {VL+210+i};
        EndFor
    EndIf
EndFor

Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";
Physical Volume("Shell_Solid") = {keep_vols[]};

Mesh.RecombineAll = 1;
Mesh.SurfaceEdges = 1;
Mesh.VolumeEdges = 1;
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; 
Mesh 3;
)";

    return script.arg(m_rIn).arg(m_wallThickness).arg(m_hCap).arg(m_hVoid).arg(m_meshSize)
        .arg(m_cx).arg(m_cy).arg(m_cz)
        .arg(m_zStart).arg(m_zEnd).arg(m_msLocalZ).arg(m_msWall);
}