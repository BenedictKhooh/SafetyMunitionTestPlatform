#include "RefinedCylinderGenerator.h"

QString RefinedCylinderGenerator::generateGeoScript() const {
    QString script = R"(
SetFactory("Built-in");

R  = %1;
H  = %2;
ms = %3;
CX = %4; CY = %5; CZ = %6;
Z_start = %7;
Z_end   = %8;
ms_local_z = %9;

If (ms_local_z <= 1e-5)
    ms_local_z = ms;
EndIf

Z_start = (Z_start < 0) ? 0 : Z_start;
Z_end   = (Z_end > H) ? H : Z_end;
Z_start = (Z_start > Z_end) ? Z_end : Z_start;

ratio = 0.707; 
L_in = R * ratio;
arc_len = Pi * R / 4.0;
nC = Max(1, Round(arc_len / ms));
actual_ms = arc_len / nC; 
nR = Max(1, Round((R - L_in) / actual_ms));

nC_nodes = nC + 1; nR_nodes = nR + 1;
A = L_in * Cos(Pi/8); B = L_in * Sin(Pi/8);
AR = R * Cos(Pi/8); BR = R * Sin(Pi/8);

Point(0) = {CX, CY, CZ}; 
Point(1) = {CX + A,  CY + B,  CZ}; Point(2) = {CX + B,  CY + A,  CZ}; 
Point(3) = {CX - B,  CY + A,  CZ}; Point(4) = {CX - A,  CY + B,  CZ};
Point(5) = {CX - A,  CY - B,  CZ}; Point(6) = {CX - B,  CY - A,  CZ}; 
Point(7) = {CX + B,  CY - A,  CZ}; Point(8) = {CX + A,  CY - B,  CZ};
Point(11)= {CX + AR, CY + BR, CZ}; Point(12)= {CX + BR, CY + AR, CZ}; 
Point(13)= {CX - BR, CY + AR, CZ}; Point(14)= {CX - AR, CY + BR, CZ};
Point(15)= {CX - AR, CY - BR, CZ}; Point(16)= {CX - BR, CY - AR, CZ};
Point(17)= {CX + BR, CY - AR, CZ}; Point(18)= {CX + AR, CY - BR, CZ};

Line(101) = {0, 1}; Line(103) = {0, 3}; Line(105) = {0, 5}; Line(107) = {0, 7};
For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Line(i) = {i, ni};
    Line(20+i) = {i, 10+i};
    Circle(30+i) = {10+i, 0, 10+ni};
EndFor

Curve Loop(1) = {101, 1, 2, -103}; Plane Surface(1) = {1}; 
Curve Loop(2) = {103, 3, 4, -105}; Plane Surface(2) = {2}; 
Curve Loop(3) = {105, 5, 6, -107}; Plane Surface(3) = {3}; 
Curve Loop(4) = {107, 7, 8, -101}; Plane Surface(4) = {4}; 
For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Curve Loop(10+i) = {20+i, 30+i, -(20+ni), -i}; Plane Surface(10+i) = {10+i}; 
EndFor

Transfinite Curve {101, 103, 105, 107} = nC_nodes;
Transfinite Curve {1:8, 31:38} = nC_nodes;
Transfinite Curve {21:28} = nR_nodes;

H1 = Z_start;
H2 = Z_end - Z_start;
H3 = H - Z_end;
base_surfs[] = {1, 2, 3, 4, 11, 12, 13, 14, 15, 16, 17, 18};

If (H1 > 1e-5)
    nL1 = Max(1, Round(H1 / ms));
    Extrude {0, 0, H1} { Surface{base_surfs[]}; Layers{nL1}; Recombine; }
    eps = 1e-3;
    next_surfs[] = Surface In BoundingBox {CX-R-eps, CY-R-eps, CZ+H1-eps, CX+R+eps, CY+R+eps, CZ+H1+eps};
Else
    next_surfs[] = base_surfs[];
EndIf

If (H2 > 1e-5)
    nL2 = Max(1, Round(H2 / ms_local_z));
    Extrude {0, 0, H2} { Surface{next_surfs[]}; Layers{nL2}; Recombine; }
    eps = 1e-3;
    next_surfs[] = Surface In BoundingBox {CX-R-eps, CY-R-eps, CZ+Z_end-eps, CX+R+eps, CY+R+eps, CZ+Z_end+eps};
EndIf

If (H3 > 1e-5)
    nL3 = Max(1, Round(H3 / ms));
    Extrude {0, 0, H3} { Surface{next_surfs[]}; Layers{nL3}; Recombine; }
EndIf

Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";
Physical Volume("Solid_Hex") = Volume "*";

Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0; 
Mesh.RecombineAll = 1;
Mesh 3;
)";

    return script.arg(m_radius).arg(m_height).arg(m_meshSize)
        .arg(m_cx).arg(m_cy).arg(m_cz)
        .arg(m_zStart).arg(m_zEnd).arg(m_msLocalZ);
}