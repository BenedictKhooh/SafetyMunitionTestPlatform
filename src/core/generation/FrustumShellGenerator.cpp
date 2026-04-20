#include "FrustumShellGenerator.h"

QString FrustumShellGenerator::buildGeoScript(double rInBot, double rInTop, double wall, double hCap, double hVoid,
    double ms, double cx, double cy, double cz, double msWall) const {
    return QString(R"(
// 强制使用内置几何引擎，稳定 O-Grid 拓扑映射
SetFactory("Built-in");

RiB = %1;
RiT = %2;
W   = %3;
Hc  = %4;
Hv  = %5;
ms  = %6;

CX = %7; CY = %8; CZ = %9;
ms_wall = %10; // ★ 新增：壁厚局部网格尺寸

H_total = 2 * Hc + Hv;
ratio = 0.707; 

// ============================================================================
// 2. 自动网格分度算法 (保障 Aspect Ratio ~ 1)
// ============================================================================
avg_R_out = (RiB + RiT) / 2.0 + W;
arc_len = Pi * avg_R_out / 4.0;
nC = Max(1, Round(arc_len / ms));
actual_ms = arc_len / nC; 

// O-Grid 内部径向分度
avg_R_in = (RiB + RiT) / 2.0;
avg_L_in = avg_R_in * ratio;
nR_in = Max(1, Round((avg_R_in - avg_L_in) / actual_ms));

// ★ 核心修改：使用专属的 ms_wall 控制壁厚的分层数量 ★
nR_wall = Max(1, Round(W / ms_wall));

nC_nodes      = nC + 1;
nR_in_nodes   = nR_in + 1;
nR_wall_nodes = nR_wall + 1;

// 垂直分段
nH_cap  = Max(1, Round(Hc / ms));
nH_void = Max(1, Round(Hv / ms));
nH_cap_nodes  = nH_cap + 1;
nH_void_nodes = nH_void + 1;

// ============================================================================
// 3. 构建 4 层切面，基于高度的动态极坐标生成
// ============================================================================
Z[] = {CZ, CZ + Hc, CZ + Hc + Hv, CZ + H_total};

For k In {0:3}
  Ri = RiB + (RiT - RiB) * (Z[k] - CZ) / H_total;
  Ro = Ri + W;
  L  = Ri * ratio;
  
  pB = k * 100;
  
  Point(pB + 0) = {CX, CY, Z[k]};
  
  For i In {1:8}
    a = (2*i - 1) * Pi / 8;
    Point(pB + i)      = {CX + L * Cos(a), CY + L * Sin(a), Z[k]};
    Point(pB + 10 + i) = {CX + Ri * Cos(a), CY + Ri * Sin(a), Z[k]};
    Point(pB + 20 + i) = {CX + Ro * Cos(a), CY + Ro * Sin(a), Z[k]};
  EndFor
  
  Line(pB + 31) = {pB+0, pB+1}; Line(pB + 33) = {pB+0, pB+3};
  Line(pB + 35) = {pB+0, pB+5}; Line(pB + 37) = {pB+0, pB+7};
  
  For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Line(pB + 40 + i) = {pB+i, pB+ni};
    Line(pB + 50 + i) = {pB+i, pB+10+i};
    Circle(pB + 60 + i) = {pB+10+i, pB+0, pB+10+ni};
    Line(pB + 70 + i) = {pB+10+i, pB+20+i};
    Circle(pB + 80 + i) = {pB+20+i, pB+0, pB+20+ni};
  EndFor
  
  sB = k * 100;
  Curve Loop(sB + 1) = {pB+31, pB+41, pB+42, -(pB+33)}; Plane Surface(sB + 1) = {sB+1};
  Curve Loop(sB + 2) = {pB+33, pB+43, pB+44, -(pB+35)}; Plane Surface(sB + 2) = {sB+2};
  Curve Loop(sB + 3) = {pB+35, pB+45, pB+46, -(pB+37)}; Plane Surface(sB + 3) = {sB+3};
  Curve Loop(sB + 4) = {pB+37, pB+47, pB+48, -(pB+31)}; Plane Surface(sB + 4) = {sB+4};
  
  For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Curve Loop(sB + 10 + i) = {pB+50+i, pB+60+i, -(pB+50+ni), -(pB+40+i)}; Plane Surface(sB + 10 + i) = {sB+10+i};
    Curve Loop(sB + 20 + i) = {pB+70+i, pB+80+i, -(pB+70+ni), -(pB+60+i)}; Plane Surface(sB + 20 + i) = {sB+20+i};
  EndFor
  
  Transfinite Curve {pB+31, pB+33, pB+35, pB+37} = nC_nodes;
  For i In {1:8}
    Transfinite Curve {pB+40+i, pB+60+i, pB+80+i} = nC_nodes;
    Transfinite Curve {pB+50+i} = nR_in_nodes;
    // ★ 这里自动继承了上方 ms_wall 算出的壁厚节点数 ★
    Transfinite Curve {pB+70+i} = nR_wall_nodes;
  EndFor
EndFor

// ============================================================================
// 4. 垂向缝合体积
// ============================================================================
vols_solid[] = {};

For k In {0:2}
    pB = k * 100; pBN = (k+1) * 100;
    sK = k * 100; sKN = (k+1) * 100;
    vsL = 1000 + k * 100;
    volL = 2000 + k * 100;
    
    If (k == 1)
        nH_nodes = nH_void_nodes;
    Else
        nH_nodes = nH_cap_nodes;
    EndIf
    
    Line(vsL + 0) = {pB+0, pBN+0}; Transfinite Curve {vsL + 0} = nH_nodes;
    For i In {1:8}
        Line(vsL + i)      = {pB+i, pBN+i};            Transfinite Curve {vsL + i}      = nH_nodes;
        Line(vsL + 10 + i) = {pB+10+i, pBN+10+i};      Transfinite Curve {vsL + 10 + i} = nH_nodes;
        Line(vsL + 20 + i) = {pB+20+i, pBN+20+i};      Transfinite Curve {vsL + 20 + i} = nH_nodes;
    EndFor
    
    Curve Loop(vsL + 1) = {pB+31, vsL+1, -(pBN+31), -(vsL+0)}; Surface(vsL + 1) = {vsL+1};
    Curve Loop(vsL + 3) = {pB+33, vsL+3, -(pBN+33), -(vsL+0)}; Surface(vsL + 3) = {vsL+3};
    Curve Loop(vsL + 5) = {pB+35, vsL+5, -(pBN+35), -(vsL+0)}; Surface(vsL + 5) = {vsL+5};
    Curve Loop(vsL + 7) = {pB+37, vsL+7, -(pBN+37), -(vsL+0)}; Surface(vsL + 7) = {vsL+7};
    
    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(vsL+10+i) = {pB+40+i, vsL+ni, -(pBN+40+i), -(vsL+i)};       Surface(vsL+10+i) = {vsL+10+i};
        Curve Loop(vsL+30+i) = {pB+50+i, vsL+10+i, -(pBN+50+i), -(vsL+i)};     Surface(vsL+30+i) = {vsL+30+i};
        Curve Loop(vsL+40+i) = {pB+60+i, vsL+10+ni, -(pBN+60+i), -(vsL+10+i)}; Surface(vsL+40+i) = {vsL+40+i};
        Curve Loop(vsL+50+i) = {pB+70+i, vsL+20+i, -(pBN+70+i), -(vsL+10+i)};  Surface(vsL+50+i) = {vsL+50+i};
        Curve Loop(vsL+60+i) = {pB+80+i, vsL+20+ni, -(pBN+80+i), -(vsL+20+i)}; Surface(vsL+60+i) = {vsL+60+i};
    EndFor
    
    // 如果是上下端盖层 (k=0 或 k=2)，生成核心实体
    If (k == 0 || k == 2)
        Surface Loop(volL+1) = {sK+1, sKN+1, vsL+1, vsL+11, vsL+12, vsL+3}; Volume(volL+1) = {volL+1};
        Surface Loop(volL+2) = {sK+2, sKN+2, vsL+3, vsL+13, vsL+14, vsL+5}; Volume(volL+2) = {volL+2};
        Surface Loop(volL+3) = {sK+3, sKN+3, vsL+5, vsL+15, vsL+16, vsL+7}; Volume(volL+3) = {volL+3};
        Surface Loop(volL+4) = {sK+4, sKN+4, vsL+7, vsL+17, vsL+18, vsL+1}; Volume(volL+4) = {volL+4};

        vols_solid[#vols_solid[]] = volL+1; vols_solid[#vols_solid[]] = volL+2;
        vols_solid[#vols_solid[]] = volL+3; vols_solid[#vols_solid[]] = volL+4;

        For i In {1:8}
            ni = (i==8) ? 1 : i+1;
            Surface Loop(volL+10+i) = {sK+10+i, sKN+10+i, vsL+40+i, vsL+20+i, vsL+40+ni, vsL+10+i};
            Volume(volL+10+i) = {volL+10+i};
            vols_solid[#vols_solid[]] = volL+10+i;
        EndFor
    EndIf
    
    // 外壳壁实体任何层都生成
    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Surface Loop(volL+20+i) = {sK+20+i, sKN+20+i, vsL+60+i, vsL+50+i, vsL+60+ni, vsL+50+ni};
        Volume(volL+20+i) = {volL+20+i};
        vols_solid[#vols_solid[]] = volL+20+i;
    EndFor
EndFor

// ============================================================================
// 5. 激活映射与清理
// ============================================================================
Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*"; Recombine Volume "*";
Physical Volume("Shell_Solid") = {vols_solid[]};

Mesh.RecombineAll = 1;
Mesh.SurfaceEdges = 1;
Mesh.VolumeEdges = 1;
Mesh 3;
)").arg(rInBot).arg(rInTop).arg(wall).arg(hCap).arg(hVoid).arg(ms).arg(cx).arg(cy).arg(cz).arg(msWall);
}