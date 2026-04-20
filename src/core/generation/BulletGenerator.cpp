#include "BulletGenerator.h"

QString BulletGenerator::buildGeoScript() const {
    // 1. 将 C++ 变量安全地注入为 Gmsh 的全局参数
    // 注意：使用 m_caliber / 2.0 转化为外半径 R_out
    QString paramsHeader = QString(
        "SetFactory(\"Built-in\");\n\n"
        "// === 1. 参数注入区 (由 C++ BulletGenerator 自动生成) ===\n"
        "R_out      = %1;\n"
        "T_jacket   = %2;\n"
        "L_cyl      = %3;\n"
        "L_nose     = %4;\n"
        "D_tip      = %5;\n"
        "ratio_core = %6;\n"
        "ms_xy      = %7;\n"
        "ms_z_cyl   = %8;\n"
        "ms_z_nose  = %9;\n"
        "prog_nose  = %10;\n"
        "CX         = %11;\n"
        "CY         = %12;\n"
        "CZ         = %13;\n\n"
    ).arg(m_caliber / 2.0)
        .arg(m_jacketThickness)
        .arg(m_cylinderLength)
        .arg(m_noseLength)
        .arg(m_tipDiameter)
        .arg(m_coreRatio)
        .arg(m_meshSizeXY)
        .arg(m_meshSizeZCyl)
        .arg(m_meshSizeZNose)
        .arg(m_progNose)
        .arg(m_cx).arg(m_cy).arg(m_cz);

    // 2. 核心拓扑生成逻辑 (无需修改内部参数，完全依赖 Header 注入的值)
    // 已经将 CX, CY, CZ 应用到 Point 的坐标生成中
    QString scriptBody = R"(
R_in = R_out - T_jacket; 

// --- 2. 卵形切线计算与渐变切片 ---
R_ogive = (R_out^2 + L_nose^2) / (2 * R_out);
Z_trunc = Sqrt(R_ogive^2 - (D_tip/2 - R_out + R_ogive)^2);

Z_slice[]  = {};
Ri_slice[] = {}; 
Ro_slice[] = {}; 

n_cyl  = Max(1, Round(L_cyl / ms_z_cyl));
n_nose = Max(1, Round(Z_trunc / ms_z_nose));

idx = 0;
For i In {0 : n_cyl}
    Z_slice[idx] = i * (L_cyl / n_cyl);
    Ri_slice[idx] = R_in;
    Ro_slice[idx] = R_out;
    idx++;
EndFor

total_ratio = (1.0 - prog_nose^n_nose) / (1.0 - prog_nose);
If(prog_nose == 1.0) total_ratio = n_nose; EndIf

For i In {1 : n_nose}
    If(prog_nose == 1.0) current_ratio = i; Else current_ratio = (1.0 - prog_nose^i) / (1.0 - prog_nose); EndIf
    z_rel = Z_trunc * (current_ratio / total_ratio);
    
    Z_slice[idx] = L_cyl + z_rel;
    Ro_cur = Sqrt(R_ogive^2 - z_rel^2) + (R_out - R_ogive);
    Ro_slice[idx] = Ro_cur;
    Ri_slice[idx] = Ro_cur * (R_in / R_out); 
    idx++;
EndFor

// --- 3. 截面节点智能分配 ---
nC = 2 * Max(1, Round(((Pi * R_in / 2.0) * ratio_core / ms_xy) / 2)); 
nR_in = Max(1, Round((R_in * (1.0 - ratio_core)) / ms_xy)); 
nR_jacket = Max(1, Round(T_jacket / ms_xy)); 

nC_nodes = nC + 1;
nR_in_nodes = nR_in + 1;
nR_jacket_nodes = nR_jacket + 1;

// --- 4. 逐层生成 20-Block 全对称拓扑 ---
For k In {0 : idx-1}
    L0 = k * 100000;
    z = Z_slice[k];
    Ri = Ri_slice[k]; Ro = Ro_slice[k];
    L = Ri * ratio_core; 
    
    // ★ 加入了空间平移偏移量 CX, CY, CZ ★
    Point(L0+0) = {CX + 0, CY + 0, CZ + z}; 
    For i In {1:8}
        a = (2*i - 1) * Pi / 8; 
        Point(L0+i)    = {CX + L*Cos(a), CY + L*Sin(a), CZ + z};   
        Point(L0+10+i) = {CX + Ri*Cos(a), CY + Ri*Sin(a), CZ + z}; 
        Point(L0+20+i) = {CX + Ro*Cos(a), CY + Ro*Sin(a), CZ + z}; 
    EndFor

    Line(L0+101) = {L0+0, L0+1}; Line(L0+103) = {L0+0, L0+3};
    Line(L0+105) = {L0+0, L0+5}; Line(L0+107) = {L0+0, L0+7};

    For i In {1:8}
        ni = (i == 8) ? 1 : i + 1;
        Line(L0+110+i)   = {L0+i, L0+ni};               
        Line(L0+120+i)   = {L0+i, L0+10+i};             
        Circle(L0+130+i) = {L0+10+i, L0+0, L0+10+ni};   
        Line(L0+140+i)   = {L0+10+i, L0+20+i};          
        Circle(L0+150+i) = {L0+20+i, L0+0, L0+20+ni};   
    EndFor

    Curve Loop(L0+201) = {L0+101, L0+111, L0+112, -(L0+103)}; Plane Surface(L0+201)={L0+201};
    Curve Loop(L0+203) = {L0+103, L0+113, L0+114, -(L0+105)}; Plane Surface(L0+203)={L0+203};
    Curve Loop(L0+205) = {L0+105, L0+115, L0+116, -(L0+107)}; Plane Surface(L0+205)={L0+205};
    Curve Loop(L0+207) = {L0+107, L0+117, L0+118, -(L0+101)}; Plane Surface(L0+207)={L0+207};

    For i In {1:8}
        ni = (i == 8) ? 1 : i + 1;
        Curve Loop(L0+210+i) = {L0+110+i, L0+120+ni, -(L0+130+i), -(L0+120+i)}; Plane Surface(L0+210+i)={L0+210+i};
        Curve Loop(L0+220+i) = {L0+130+i, L0+140+ni, -(L0+150+i), -(L0+140+i)}; Plane Surface(L0+220+i)={L0+220+i};
    EndFor

    Transfinite Surface{L0+201} = {L0+0, L0+1, L0+2, L0+3};
    Transfinite Surface{L0+203} = {L0+0, L0+3, L0+4, L0+5};
    Transfinite Surface{L0+205} = {L0+0, L0+5, L0+6, L0+7};
    Transfinite Surface{L0+207} = {L0+0, L0+7, L0+8, L0+1};
    
    Transfinite Curve {L0+101, L0+103, L0+105, L0+107} = nC_nodes;
    For i In {1:8}
        ni = (i == 8) ? 1 : i + 1;
        Transfinite Surface{L0+210+i} = {L0+i, L0+ni, L0+10+ni, L0+10+i};
        Transfinite Surface{L0+220+i} = {L0+10+i, L0+10+ni, L0+20+ni, L0+20+i};
        Transfinite Curve {L0+110+i, L0+130+i, L0+150+i} = nC_nodes;
        Transfinite Curve {L0+120+i} = nR_in_nodes;
        Transfinite Curve {L0+140+i} = nR_jacket_nodes;
    EndFor
EndFor

// --- 5. 纵向严密体积封装 ---
core_vols[] = {}; jacket_vols[] = {};

For k In {0 : idx-2}
    L0 = k * 100000; L1 = (k+1) * 100000;
    
    Line(L0+300) = {L0+0, L1+0}; Transfinite Curve {L0+300} = 2;
    For i In {1:8}
        Line(L0+300+i) = {L0+i, L1+i};       Transfinite Curve {L0+300+i} = 2;
        Line(L0+310+i) = {L0+10+i, L1+10+i}; Transfinite Curve {L0+310+i} = 2;
        Line(L0+320+i) = {L0+20+i, L1+20+i}; Transfinite Curve {L0+320+i} = 2;
    EndFor

    Curve Loop(L0+401) = {L0+101, L0+301, -(L1+101), -(L0+300)}; Surface(L0+401)={L0+401}; Transfinite Surface{L0+401}={L0+0, L0+1, L1+1, L1+0};
    Curve Loop(L0+403) = {L0+103, L0+303, -(L1+103), -(L0+300)}; Surface(L0+403)={L0+403}; Transfinite Surface{L0+403}={L0+0, L0+3, L1+3, L1+0};
    Curve Loop(L0+405) = {L0+105, L0+305, -(L1+105), -(L0+300)}; Surface(L0+405)={L0+405}; Transfinite Surface{L0+405}={L0+0, L0+5, L1+5, L1+0};
    Curve Loop(L0+407) = {L0+107, L0+307, -(L1+107), -(L0+300)}; Surface(L0+407)={L0+407}; Transfinite Surface{L0+407}={L0+0, L0+7, L1+7, L1+0};

    For i In {1:8}
        ni = (i == 8) ? 1 : i + 1;
        Curve Loop(L0+410+i) = {L0+110+i, L0+300+ni, -(L1+110+i), -(L0+300+i)}; Surface(L0+410+i)={L0+410+i}; Transfinite Surface{L0+410+i}={L0+i, L0+ni, L1+ni, L1+i};
        Curve Loop(L0+420+i) = {L0+120+i, L0+310+i, -(L1+120+i), -(L0+300+i)};  Surface(L0+420+i)={L0+420+i}; Transfinite Surface{L0+420+i}={L0+i, L0+10+i, L1+10+i, L1+i};
        Curve Loop(L0+430+i) = {L0+130+i, L0+310+ni, -(L1+130+i), -(L0+310+i)}; Surface(L0+430+i)={L0+430+i}; Transfinite Surface{L0+430+i}={L0+10+i, L0+10+ni, L1+10+ni, L1+10+i};
        Curve Loop(L0+440+i) = {L0+140+i, L0+320+i, -(L1+140+i), -(L0+310+i)};  Surface(L0+440+i)={L0+440+i}; Transfinite Surface{L0+440+i}={L0+10+i, L0+20+i, L1+20+i, L1+10+i};
        Curve Loop(L0+450+i) = {L0+150+i, L0+320+ni, -(L1+150+i), -(L0+320+i)}; Surface(L0+450+i)={L0+450+i}; Transfinite Surface{L0+450+i}={L0+20+i, L0+20+ni, L1+20+ni, L1+20+i};
    EndFor

    Surface Loop(L0+501) = {L0+201, -(L1+201), L0+401, L0+411, L0+412, -(L0+403)}; Volume(L0+501)={L0+501};
    Surface Loop(L0+503) = {L0+203, -(L1+203), L0+403, L0+413, L0+414, -(L0+405)}; Volume(L0+503)={L0+503};
    Surface Loop(L0+505) = {L0+205, -(L1+205), L0+405, L0+415, L0+416, -(L0+407)}; Volume(L0+505)={L0+505};
    Surface Loop(L0+507) = {L0+207, -(L1+207), L0+407, L0+417, L0+418, -(L0+401)}; Volume(L0+507)={L0+507};
    core_vols[] += {L0+501, L0+503, L0+505, L0+507};

    For i In {1:8}
        ni = (i == 8) ? 1 : i + 1;
        Surface Loop(L0+510+i) = {L0+210+i, -(L1+210+i), L0+410+i, L0+420+ni, -(L0+430+i), -(L0+420+i)}; Volume(L0+510+i)={L0+510+i};
        Surface Loop(L0+520+i) = {L0+220+i, -(L1+220+i), L0+430+i, L0+440+ni, -(L0+450+i), -(L0+440+i)}; Volume(L0+520+i)={L0+520+i};
        core_vols[] += {L0+510+i};
        jacket_vols[] += {L0+520+i};
    EndFor
EndFor

// --- 6. 物理分区 ---
Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";

Physical Volume("Bullet_Core", 1) = {core_vols[]};
Physical Volume("Bullet_Jacket", 2) = {jacket_vols[]};

Mesh.RecombineAll = 1;
Mesh.SurfaceEdges = 1;
Mesh.VolumeEdges  = 1;
Mesh.MshFileVersion = 2.2;
Mesh.SaveAll = 0;
)";

    return paramsHeader + scriptBody;
}