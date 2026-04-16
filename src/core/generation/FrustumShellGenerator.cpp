#include "FrustumShellGenerator.h"

QString FrustumShellGenerator::buildGeoScript(double rInBot, double rInTop, double wall, double hCap, double hVoid,
    double ms, double cx, double cy, double cz) const {
    return QString(R"(
// 强制使用内置几何引擎
SetFactory("Built-in");

// ==========================================
// 1. 基础参数定义 (C++ 自动注入)
// ==========================================
R_in_bot = %1;
R_in_top = %2;
Wall     = %3;
H_cap    = %4;
H_void   = %5;
ms       = %6;

CX = %7; CY = %8; CZ = %9;

H_total = 2 * H_cap + H_void;

// O-Grid 核心尺寸比例
ratio = 0.55; 

// --- 自动网格分度算法：等距投影 (保证 Aspect Ratio ~ 1) ---
avg_R_out = (R_in_bot + R_in_top) / 2.0 + Wall;
nC = Max(2, Round((Pi * avg_R_out / 2.0) / ms));
actual_ms = (Pi * avg_R_out / 2.0) / nC;

// 根据真实弧长反推各方向网格数
nR_wall = Max(1, Round(Wall / actual_ms));
nR_in   = Max(2, Round((R_in_bot * ratio) / actual_ms));
nR_void = Max(1, Round((R_in_bot * (1 - ratio)) / actual_ms));
nH_cap  = Max(1, Round(H_cap / actual_ms));
nH_void = Max(2, Round(H_void / actual_ms));

nC_nodes      = nC + 1;
nR_in_nodes   = nR_in + 1;
nR_wall_nodes = nR_wall + 1;
nR_void_nodes = nR_void + 1;
nH_cap_nodes  = nH_cap + 1;
nH_void_nodes = nH_void + 1;

// Z轴高度分层与垂直网格层数数组
Z_arr[0] = CZ;
Z_arr[1] = CZ + H_cap;
Z_arr[2] = CZ + H_cap + H_void;
Z_arr[3] = CZ + H_total;

nLayers_arr[0] = nH_cap_nodes;
nLayers_arr[1] = nH_void_nodes;
nLayers_arr[2] = nH_cap_nodes;

// 存储最终实体
vols_solid[] = {};

// ==========================================
// 2. 宏定义：生成单层截面的八边形点、线、面
// ==========================================
Macro BuildLevel
    pK = 1000 + K * 100;
    lK = 2000 + K * 100;
    sK = 3000 + K * 100;
    
    // 当前高度对应的半径
    ri = R_in_bot + (R_in_top - R_in_bot) * ((Z_arr[K] - CZ) / H_total);
    ro = ri + Wall;
    li = ri * ratio; 
    
    Point(pK + 0) = {CX, CY, Z_arr[K]}; 
    
    // 2.1 阵列化生成 3 个同心圈的点
    For i In {1:8}
        a = (2*i - 1) * Pi / 8;
        Point(pK + i) = {CX + li * Cos(a), CY + li * Sin(a), Z_arr[K]};           // 内圈 (八边形)
        Point(pK + 10 + i) = {CX + ri * Cos(a), CY + ri * Sin(a), Z_arr[K]};      // 中圈 (内腔壁)
        Point(pK + 20 + i) = {CX + ro * Cos(a), CY + ro * Sin(a), Z_arr[K]};      // 外圈 (外壳壁)
    EndFor

    // 2.2 阵列化生成横截面网格线
    Line(lK + 1) = {pK + 0, pK + 1}; Line(lK + 3) = {pK + 0, pK + 3};
    Line(lK + 5) = {pK + 0, pK + 5}; Line(lK + 7) = {pK + 0, pK + 7};

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Line(lK + 10 + i)   = {pK + i, pK + ni};                       
        Circle(lK + 20 + i) = {pK + 10 + i, pK + 0, pK + 10 + ni};   
        Circle(lK + 30 + i) = {pK + 20 + i, pK + 0, pK + 20 + ni};   
        Line(lK + 40 + i)   = {pK + i, pK + 10 + i};                   
        Line(lK + 50 + i)   = {pK + 10 + i, pK + 20 + i};              
    EndFor

    // 2.3 生成横截面平面
    Curve Loop(sK + 1) = {lK+1, lK+11, lK+12, -(lK+3)}; Plane Surface(sK + 1) = {sK + 1};
    Curve Loop(sK + 2) = {lK+3, lK+13, lK+14, -(lK+5)}; Plane Surface(sK + 2) = {sK + 2};
    Curve Loop(sK + 3) = {lK+5, lK+15, lK+16, -(lK+7)}; Plane Surface(sK + 3) = {sK + 3};
    Curve Loop(sK + 4) = {lK+7, lK+17, lK+18, -(lK+1)}; Plane Surface(sK + 4) = {sK + 4};

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(sK + 10 + i) = {lK+40+i, lK+20+i, -(lK+40+ni), -(lK+10+i)}; Plane Surface(sK + 10 + i) = {sK + 10 + i};
        Curve Loop(sK + 20 + i) = {lK+50+i, lK+30+i, -(lK+50+ni), -(lK+20+i)}; Plane Surface(sK + 20 + i) = {sK + 20 + i};
    EndFor

    // 2.4 施加结构化点阵约束
    Transfinite Curve {lK+1, lK+3, lK+5, lK+7} = nC_nodes;
    Transfinite Curve {lK+11:lK+18, lK+21:lK+28, lK+31:lK+38} = nC_nodes;
    Transfinite Curve {lK+41:lK+48} = nR_void_nodes; // O-Grid内外径向约束
    Transfinite Curve {lK+51:lK+58} = nR_wall_nodes; // 壁厚径向约束
Return

// ==========================================
// 3. 宏定义：层间拉伸与体生成
// ==========================================
Macro BuildLayer
    vL = 4000 + L * 100;
    vsL= 5000 + L * 100;
    volL=6000 + L * 100;
    pK = 1000 + L * 100;
    pKN= 1000 + (L+1) * 100;
    lK = 2000 + L * 100;
    lKN= 2000 + (L+1) * 100;
    sK = 3000 + L * 100;
    sKN= 3000 + (L+1) * 100;
    nl = nLayers_arr[L];

    // ----------------------------------------------------
    // 3.1 永远存在的外部侧壁 (外壁、内腔壁、隔离板)
    // ----------------------------------------------------
    For i In {1:8}
        Line(vL + 20 + i) = {pK + 20 + i, pKN + 20 + i}; 
        Line(vL + 10 + i) = {pK + 10 + i, pKN + 10 + i}; 
    EndFor
    Transfinite Curve {vL+21:vL+28, vL+11:vL+18} = nl;

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(vsL+30+i) = {lK+30+i, vL+20+ni, -(lKN+30+i), -(vL+20+i)}; Surface(vsL+30+i) = {vsL+30+i};
        Curve Loop(vsL+20+i) = {lK+20+i, vL+10+ni, -(lKN+20+i), -(vL+10+i)}; Surface(vsL+20+i) = {vsL+20+i};
        Curve Loop(vsL+50+i) = {lK+50+i, vL+20+i, -(lKN+50+i), -(vL+10+i)}; Surface(vsL+50+i) = {vsL+50+i};
    EndFor

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Surface Loop(volL+20+i) = {sK+20+i, sKN+20+i, vsL+50+i, vsL+30+i, vsL+50+ni, vsL+20+i};
        Volume(volL+20+i) = {volL+20+i};
        vols_solid[#vols_solid[]] = volL+20+i; 
    EndFor

    // ----------------------------------------------------
    // 3.2 智能判断：如果是端盖层，才生成内部实心结构
    // ----------------------------------------------------
    If (L == 0 || L == 2)
        Line(vL + 0) = {pK + 0, pKN + 0}; 
        For i In {1:8}
            Line(vL + i) = {pK + i, pKN + i}; 
        EndFor
        Transfinite Curve {vL+0, vL+1:vL+8} = nl;

        Curve Loop(vsL+1) = {lK+1, vL+1, -(lKN+1), -(vL+0)}; Surface(vsL+1) = {vsL+1};
        Curve Loop(vsL+3) = {lK+3, vL+3, -(lKN+3), -(vL+0)}; Surface(vsL+3) = {vsL+3};
        Curve Loop(vsL+5) = {lK+5, vL+5, -(lKN+5), -(vL+0)}; Surface(vsL+5) = {vsL+5};
        Curve Loop(vsL+7) = {lK+7, vL+7, -(lKN+7), -(vL+0)}; Surface(vsL+7) = {vsL+7};

        For i In {1:8}
            ni = (i==8) ? 1 : i+1;
            Curve Loop(vsL+10+i) = {lK+10+i, vL+ni, -(lKN+10+i), -(vL+i)}; Surface(vsL+10+i) = {vsL+10+i};
            Curve Loop(vsL+40+i) = {lK+40+i, vL+10+i, -(lKN+40+i), -(vL+i)}; Surface(vsL+40+i) = {vsL+40+i};
        EndFor

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
Return

// ==========================================
// 4. 执行状态机生成
// ==========================================
For K In {0:3}
    Call BuildLevel;
EndFor

For L In {0:2}
    Call BuildLayer;
EndFor

// 全局强制结构化重组约束
Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";

// 限定导出域：仅输出带有实体网格的结构，抛弃纯粹的空腔
Physical Volume("Frustum_Shell_With_Caps") = {vols_solid[]};

Mesh.RecombineAll = 1;
Mesh.SaveAll = 0;   
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )").arg(rInBot).arg(rInTop).arg(wall).arg(hCap).arg(hVoid)
        .arg(ms).arg(cx).arg(cy).arg(cz);
}