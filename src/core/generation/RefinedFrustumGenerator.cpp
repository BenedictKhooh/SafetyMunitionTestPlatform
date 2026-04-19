#include "RefinedFrustumGenerator.h"

QString RefinedFrustumGenerator::generateGeoScript() const {
    QString script = R"(
SetFactory("Built-in");

R_base = %1; R_top = %2; H = %3; ms = %4;
CX = %5; CY = %6; CZ = %7;
Z_start = %8; Z_end = %9; ms_local_z = %10;

If (ms_local_z <= 1e-5)
    ms_local_z = ms;
EndIf

ratio = 0.55; 
Z_start = (Z_start < 0) ? 0 : Z_start;
Z_end   = (Z_end > H) ? H : Z_end;
Z_start = (Z_start > Z_end) ? Z_end : Z_start;

idx = 0; Z[0] = 0; R[0] = R_base;
If (Z_start > 1e-3)
    idx++; Z[idx] = Z_start; R[idx] = R_base + (R_top - R_base) * (Z_start / H); sz[idx-1] = ms;
EndIf
If (Z_end > Z_start + 1e-3 && Z_end < H - 1e-3)
    idx++; Z[idx] = Z_end; R[idx] = R_base + (R_top - R_base) * (Z_end / H); sz[idx-1] = ms_local_z; 
EndIf
idx++; Z[idx] = H; R[idx] = R_top;
If (Z_end >= H - 1e-3 && Z_start < H - 1e-3)
    sz[idx-1] = ms_local_z; 
Else
    sz[idx-1] = ms;         
EndIf

For k In {0 : idx-1}
    delta_R = Fabs(R[k] - R[k+1]);
    L_slant = Sqrt((Z[k+1] - Z[k])*(Z[k+1] - Z[k]) + delta_R*delta_R);
    nH[k] = Max(1, Round(L_slant / sz[k]));
EndFor

arc_len = Pi * R_base / 4.0;
nC = Max(1, Round(arc_len / ms));
nR = Max(1, Round((R_base - R_base*ratio) / ms));

For k In {0 : idx}
    pb = k * 1000;
    Point(pb) = {CX, CY, CZ + Z[k]};
    For i In {1:8}
        a = (2*i - 1) * Pi / 8; Lk = R[k] * ratio;
        Point(pb + i) = {CX + Lk*Cos(a), CY + Lk*Sin(a), CZ + Z[k]};
        Point(pb + 10 + i) = {CX + R[k]*Cos(a), CY + R[k]*Sin(a), CZ + Z[k]};
    EndFor
    Line(pb + 31) = {pb, pb + 1}; Line(pb + 33) = {pb, pb + 3};
    Line(pb + 35) = {pb, pb + 5}; Line(pb + 37) = {pb, pb + 7};
    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Line(pb + 40 + i) = {pb+i, pb+ni};               
        Line(pb + 50 + i) = {pb+i, pb+10+i};
        Circle(pb + 60 + i) = {pb+10+i, pb, pb+10+ni};    
    EndFor
    Curve Loop(pb + 101) = {pb+31, pb+41, pb+42, -(pb+33)}; Plane Surface(pb + 101) = {pb+101};
    Curve Loop(pb + 102) = {pb+33, pb+43, pb+44, -(pb+35)}; Plane Surface(pb + 102) = {pb+102};
    Curve Loop(pb + 103) = {pb+35, pb+45, pb+46, -(pb+37)}; Plane Surface(pb + 103) = {pb+103};
    Curve Loop(pb + 104) = {pb+37, pb+47, pb+48, -(pb+31)}; Plane Surface(pb + 104) = {pb+104};
    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(pb + 110 + i) = {pb+50+i, pb+60+i, -(pb+50+ni), -(pb+40+i)}; Plane Surface(pb + 110 + i) = {pb+110+i};
    EndFor

    Transfinite Curve {pb+31, pb+33, pb+35, pb+37} = nC + 1;
    For i In {1:8}
        Transfinite Curve {pb+40+i, pb+60+i} = nC + 1;
        Transfinite Curve {pb+50+i} = nR + 1;
    EndFor
EndFor

vol_ids[] = {}; 
For k In {0 : idx-1}
    pb = k * 1000; pt = (k+1) * 1000; vb = k * 10000;
    Line(vb + 50) = {pb, pt};
    For i In {1:8}
        Line(vb + 200 + i) = {pb + i, pt + i};
        Line(vb + 210 + i) = {pb + 10 + i, pt + 10 + i};
    EndFor
    Transfinite Curve {vb+50} = nH[k] + 1;
    For i In {1:8}
        Transfinite Curve {vb+200+i, vb+210+i} = nH[k] + 1;
    EndFor

    Curve Loop(vb + 501) = {pb+31, vb+201, -(pt+31), -(vb+50)}; Surface(vb + 501) = {vb+501};
    Curve Loop(vb + 503) = {pb+33, vb+203, -(pt+33), -(vb+50)}; Surface(vb + 503) = {vb+503};
    Curve Loop(vb + 505) = {pb+35, vb+205, -(pt+35), -(vb+50)}; Surface(vb + 505) = {vb+505};
    Curve Loop(vb + 507) = {pb+37, vb+207, -(pt+37), -(vb+50)}; Surface(vb + 507) = {vb+507};
    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(vb + 800+i) = {pb+40+i, vb+200+ni, -(pt+40+i), -(vb+200+i)}; Surface(vb + 800+i) = {vb+800+i};
        Curve Loop(vb + 900+i) = {pb+50+i, vb+210+i, -(pt+50+i), -(vb+200+i)}; Surface(vb + 900+i) = {vb+900+i};
        Curve Loop(vb + 1000+i)= {pb+60+i, vb+210+ni, -(pt+60+i), -(vb+210+i)}; Surface(vb + 1000+i)= {vb+1000+i};
    EndFor
    
    Surface Loop(vb + 2001) = {pb+101, pt+101, vb+501, vb+503, vb+801, vb+802}; Volume(vb + 2001) = {vb+2001};
    Surface Loop(vb + 2002) = {pb+102, pt+102, vb+503, vb+505, vb+803, vb+804}; Volume(vb + 2002) = {vb+2002};
    Surface Loop(vb + 2003) = {pb+103, pt+103, vb+505, vb+507, vb+805, vb+806}; Volume(vb + 2003) = {vb+2003};
    Surface Loop(vb + 2004) = {pb+104, pt+104, vb+507, vb+501, vb+807, vb+808}; Volume(vb + 2004) = {vb+2004};
    vol_ids[] += {vb + 2001, vb + 2002, vb + 2003, vb + 2004};

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Surface Loop(vb + 2100+i) = {pb+110+i, pt+110+i, vb+900+i, vb+900+ni, vb+1000+i, vb+800+i};
        Volume(vb + 2100+i) = {vb+2100+i};
        vol_ids[] += {vb + 2100+i};
    EndFor
EndFor

Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";
Physical Volume("Solid_Hex") = {vol_ids[]};

Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; 
Mesh.RecombineAll = 1;
Mesh 3;
)";

    return script.arg(m_rBase).arg(m_rTop).arg(m_height).arg(m_meshSize)
        .arg(m_cx).arg(m_cy).arg(m_cz)
        .arg(m_zStart).arg(m_zEnd).arg(m_msLocalZ);
}