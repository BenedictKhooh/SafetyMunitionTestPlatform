#include "FrustumShellGenerator.h"

QString FrustumShellGenerator::buildGeoScript(double rInBot, double rInTop, double wall, double hCap, double hVoid,
    double ms, double cx, double cy, double cz) const {
    return QString(R"(
// 初始化几何引擎模式
SetFactory("Built-in");

// ============================================================================
// 1. 几何与网格离散参数初始化
// ============================================================================
R_in_bot = %1;       
R_in_top = %2;       
Wall     = %3;       

H_cap  = %4;         
H_void = %5;         
ms = %6;

CX = %7; CY = %8; CZ = %9;

H_total = 2 * H_cap + H_void; 

// 网格离散密度计算 (节点数 = 单元数 + 1)
nC_nodes      = Max(2, Round((Pi * (R_in_bot + Wall) / 2) / ms)) + 1; 
nR_in_nodes   = Max(2, Round(R_in_bot / ms)) + 1;  
nR_wall_nodes = Max(2, Round(Wall / ms)) + 1;  
nH_cap_nodes  = Max(2, Round(H_cap / ms)) + 1;  
nH_void_nodes = Max(2, Round(H_void / ms)) + 1; 

// 轴向高度分层基准 (考虑平移偏置 CZ)
Z_arr[0] = CZ;
Z_arr[1] = CZ + H_cap;
Z_arr[2] = CZ + H_cap + H_void;
Z_arr[3] = CZ + H_total;

// 各层级垂直向网格节点数映射
nLayers_arr[0] = nH_cap_nodes;
nLayers_arr[1] = nH_void_nodes;
nLayers_arr[2] = nH_cap_nodes;

// 初始化实体集合数组
vols_solid[] = {};

// ============================================================================
// 2. 宏定义：构建单层 2D 横截面拓扑
// ============================================================================
Macro BuildLevel
    pK = 1000 + K * 100;
    lK = 2000 + K * 100;
    sK = 3000 + K * 100;
    
    // 线性插值计算当前 Z 高度对应的内外半径
    ri = R_in_bot + (R_in_top - R_in_bot) * ((Z_arr[K] - CZ) / H_total);
    ro = ri + Wall;
    li = ri * 0.5; // O-Grid 内部八边形特征半径系数约束
    
    // 截面几何中心点
    Point(pK + 0) = {CX, CY, Z_arr[K]}; 
    
    // 极坐标系生成环向特征点
    For i In {1:8}
        a = (2*i - 1) * Pi / 8;
        Point(pK + i)      = {CX + li * Cos(a), CY + li * Sin(a), Z_arr[K]}; // 内环
        Point(pK + 10 + i) = {CX + ri * Cos(a), CY + ri * Sin(a), Z_arr[K]}; // 中环 (内腔界)
        Point(pK + 20 + i) = {CX + ro * Cos(a), CY + ro * Sin(a), Z_arr[K]}; // 外环 (外壁界)
    EndFor

    // 生成径向特征线与弧线
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

    // 定义二维表面封闭域
    Curve Loop(sK + 1) = {lK+1, lK+11, lK+12, -(lK+3)}; Plane Surface(sK + 1) = {sK + 1};
    Curve Loop(sK + 2) = {lK+3, lK+13, lK+14, -(lK+5)}; Plane Surface(sK + 2) = {sK + 2};
    Curve Loop(sK + 3) = {lK+5, lK+15, lK+16, -(lK+7)}; Plane Surface(sK + 3) = {sK + 3};
    Curve Loop(sK + 4) = {lK+7, lK+17, lK+18, -(lK+1)}; Plane Surface(sK + 4) = {sK + 4};

    For i In {1:8}
        ni = (i==8) ? 1 : i+1;
        Curve Loop(sK + 10 + i) = {lK+40+i, lK+20+i, -(lK+40+ni), -(lK+10+i)}; Plane Surface(sK + 10 + i) = {sK + 10 + i};
        Curve Loop(sK + 20 + i) = {lK+50+i, lK+30+i, -(lK+50+ni), -(lK+20+i)}; Plane Surface(sK + 20 + i) = {sK + 20 + i};
    EndFor

    // 施加结构化网格节点约束
    Transfinite Curve {lK+1, lK+3, lK+5, lK+7} = nC_nodes;
    Transfinite Curve {lK+11:lK+18, lK+21:lK+28, lK+31:lK+38} = nC_nodes;
    Transfinite Curve {lK+41:lK+48} = nR_in_nodes;
    Transfinite Curve {lK+51:lK+58} = nR_wall_nodes;
Return

// ============================================================================
// 3. 宏定义：层间垂直拉伸与 3D 体积装配
// ============================================================================
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
    // 3.1 外侧环形壁厚体积生成 (全层均需构建)
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
    // 3.2 内部实心体积生成 (仅限于底层与顶层端盖区域)
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

// ============================================================================
// 4. 执行状态机与数据导出过滤
// ============================================================================
For K In {0:3}
    Call BuildLevel;
EndFor

For L In {0:2}
    Call BuildLayer;
EndFor

// 全局强制结构化重组约束
Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";

// 物理组限定导出域：屏蔽未定义的空腔网格体
Physical Volume("Frustum_Shell_With_Caps") = {vols_solid[]};

// 求解器导出配置 (限定 Type 5 一阶六面体)
Mesh.RecombineAll = 1;
Mesh.SaveAll = 0;   
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )").arg(rInBot).arg(rInTop).arg(wall).arg(hCap).arg(hVoid)
        .arg(ms).arg(cx).arg(cy).arg(cz);
}