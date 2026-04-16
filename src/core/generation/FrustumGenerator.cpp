#include "FrustumGenerator.h"

QString FrustumGenerator::buildGeoScript(double rBase, double rTop, double height, double meshSize, double cx, double cy, double cz) const {
    return QString(R"(
// 强制使用内置几何引擎
SetFactory("Built-in");

// ==========================================
// 1. 基础参数定义
// ==========================================
R_base = %1; R_top = %2; H = %3;
ms = %4;
CX = %5; CY = %6; CZ = %7;

// O-Grid 八边形内圈控制比例
ratio = 0.55; 

// ==========================================
// 2. 动态自适应等距网格分段 (Aspect Ratio ~ 1)
// ==========================================
// 2.1 环向分度：八边形单段弧长应为 2*Pi*R / 8 = Pi*R / 4
arc_len = Pi * R_base / 4.0;
nC = Max(1, Round(arc_len / ms));
actual_ms = arc_len / nC; // 反推真实弧长作为全局网格基准

// 2.2 径向分度
nR = Max(1, Round((R_base * (1 - ratio)) / actual_ms));

// 2.3 垂向分度 (基于真实斜边长)
delta_R = Fabs(R_base - R_top);
L_slant = Sqrt(H * H + delta_R * delta_R);
nH = Max(1, Round(L_slant / actual_ms));

r_height = 1.0; 
L_b = R_base * ratio; 
L_t = R_top * ratio;

// 转换为节点数
nC_nodes = nC + 1;
nR_nodes = nR + 1;
nH_nodes = nH + 1;

// ==========================================
// 3. 阵列化顶点生成
// ==========================================
Point(0) = {CX, CY, CZ};           
Point(1000) = {CX, CY, CZ + H};    

For i In {1:8}
    a = (2*i - 1) * Pi / 8;
    
    // 底面: 1~8 (内), 11~18 (外)
    Point(i) = {CX + L_b*Cos(a), CY + L_b*Sin(a), CZ};
    Point(i+10) = {CX + R_base*Cos(a), CY + R_base*Sin(a), CZ};
    
    // 顶面: 101~108 (内), 111~118 (外)
    Point(i+100) = {CX + L_t*Cos(a), CY + L_t*Sin(a), CZ + H};
    Point(i+110) = {CX + R_top*Cos(a), CY + R_top*Sin(a), CZ + H};
EndFor

// ==========================================
// 4. 阵列化线框生成
// ==========================================
Line(50) = {0, 1000};
Line(31) = {0, 1}; Line(33) = {0, 3}; Line(35) = {0, 5}; Line(37) = {0, 7};
Line(131)= {1000, 101}; Line(133)= {1000, 103}; Line(135)= {1000, 105}; Line(137)= {1000, 107};

For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Line(i) = {i, ni};
    Circle(i+10) = {i+10, 0, ni+10};
    Line(i+20) = {i, i+10};
    
    Line(i+100) = {i+100, ni+100};
    Circle(i+110) = {i+110, 1000, ni+110};
    Line(i+120) = {i+100, i+110};
    
    Line(i+200) = {i, i+100};
    Line(i+210) = {i+10, i+110};
EndFor

// ==========================================
// 5. 封闭面生成
// ==========================================
Curve Loop(501) = {31, 201, -131, -50}; Plane Surface(501) = {501};
Curve Loop(503) = {33, 203, -133, -50}; Plane Surface(503) = {503};
Curve Loop(505) = {35, 205, -135, -50}; Plane Surface(505) = {505};
Curve Loop(507) = {37, 207, -137, -50}; Plane Surface(507) = {507};

Curve Loop(301) = {31, 1, 2, -33}; Plane Surface(301) = {301};
Curve Loop(302) = {33, 3, 4, -35}; Plane Surface(302) = {302};
Curve Loop(303) = {35, 5, 6, -37}; Plane Surface(303) = {303};
Curve Loop(304) = {37, 7, 8, -31}; Plane Surface(304) = {304};

Curve Loop(401) = {131, 101, 102, -133}; Plane Surface(401) = {401};
Curve Loop(402) = {133, 103, 104, -135}; Plane Surface(402) = {402};
Curve Loop(403) = {135, 105, 106, -137}; Plane Surface(403) = {403};
Curve Loop(404) = {137, 107, 108, -131}; Plane Surface(404) = {404};

For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Curve Loop(600+i) = {i+20, i+10, -(ni+20), -i};          Plane Surface(600+i) = {600+i};
    Curve Loop(700+i) = {i+120, i+110, -(ni+120), -(i+100)}; Plane Surface(700+i) = {700+i};
    Curve Loop(800+i) = {i, 200+ni, -(i+100), -(200+i)};     Surface(800+i) = {800+i};
    Curve Loop(900+i) = {i+10, 210+ni, -(i+110), -(210+i)};  Surface(900+i) = {900+i};
    Curve Loop(1000+i)= {i+20, 210+i, -(i+120), -(200+i)};   Surface(1000+i) = {1000+i};
EndFor

// ==========================================
// 6. 完美组装 12 个体积
// ==========================================
Surface Loop(2001) = {301, 401, 501, 503, 801, 802}; Volume(2001) = {2001};
Surface Loop(2002) = {302, 402, 503, 505, 803, 804}; Volume(2002) = {2002};
Surface Loop(2003) = {303, 403, 505, 507, 805, 806}; Volume(2003) = {2003};
Surface Loop(2004) = {304, 404, 507, 501, 807, 808}; Volume(2004) = {2004};

For i In {1:8}
    ni = (i==8) ? 1 : i+1;
    Surface Loop(2100+i) = {600+i, 700+i, 1000+i, 1000+ni, 800+i, 900+i};
    Volume(2100+i) = {2100+i};
EndFor

// ==========================================
// 7. 结构化网格点阵映射
// ==========================================
Transfinite Curve {1:8, 11:18, 101:108, 111:118} = nC_nodes;
Transfinite Curve {31, 33, 35, 37, 131, 133, 135, 137} = nC_nodes;
Transfinite Curve {21:28, 121:128} = nR_nodes;
Transfinite Curve {50, 201:208, 211:218} = nH_nodes Using Progression r_height;

// ==========================================
// 8. 纯粹 Type 5 导出机制
// ==========================================
Transfinite Surface "*"; Recombine Surface "*";
Transfinite Volume "*";  Recombine Volume "*";

// 手动拼接时不会出现 ID 逃逸，直接导出对应的体积即可
Physical Volume("Solid_Hex") = {2001:2004, 2101:2108};

Mesh.RecombineAll = 1;
Mesh.SaveAll = 0;   
Mesh.ElementOrder = 1;
Mesh.MshFileVersion = 2.2;
Mesh 3;
    )")
        .arg(rBase).arg(rTop).arg(height).arg(meshSize)
        .arg(cx).arg(cy).arg(cz);
}