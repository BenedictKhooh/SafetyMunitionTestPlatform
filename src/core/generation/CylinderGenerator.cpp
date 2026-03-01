#include "CylinderGenerator.h"

QString CylinderGenerator::buildGeoScript(double radius, double meshSize, double height, double cx, double cy, double cz) const {

	int nC = 2 * round((radius / (2 * 1.414)) / meshSize);
	int nH = round(height / meshSize);
	return QString(R"(
////////////////////////////////////////////////////
// Cylindrical O-grid mesh (parameterized)
////////////////////////////////////////////////////

// 1. 参数定义
R = %1;
H = %2;

nC = %3;
nH = %4;

L = R /(2 * 1.414);
Rproj = R / 1.414;
nR = nC / 1.414;

Printf("L = %g, R = %g, H = %g", L, R, H);

// 2. 点定义
Point(1)  = {0, 0, 0};

Point(2)  = { L,  L, 0};
Point(3)  = {-L,  L, 0};
Point(4)  = {-L, -L, 0};
Point(5)  = { L, -L, 0};

Point(6)  = { Rproj,  Rproj, 0};
Point(7)  = {-Rproj,  Rproj, 0};
Point(8)  = {-Rproj, -Rproj, 0};
Point(9)  = { Rproj, -Rproj, 0};

Point(10) = { 2*L, 0, 0};
Point(11) = { 0, 2*L, 0};
Point(12) = {-2*L, 0, 0};
Point(13) = { 0,-2*L, 0};

// 3. 线定义
Circle(1) = {2, 13, 3};
Circle(2) = {3, 10, 4};
Circle(3) = {4, 11, 5};
Circle(4) = {5, 12, 2};

Circle(5) = {6, 1, 7};
Circle(6) = {7, 1, 8};
Circle(7) = {8, 1, 9};
Circle(8) = {9, 1, 6};

Line(9)  = {2, 6};
Line(10) = {3, 7};
Line(11) = {4, 8};
Line(12) = {5, 9};

// 4. 面定义
Curve Loop(1) = {1, 2, 3, 4};
Plane Surface(1) = {1};

Curve Loop(2) = {9, 5, -10, -1};
Plane Surface(2) = {2};

Curve Loop(3) = {10, 6, -11, -2};
Plane Surface(3) = {3};

Curve Loop(4) = {11, 7, -12, -3};
Plane Surface(4) = {4};

Curve Loop(5) = {12, 8, -9, -4};
Plane Surface(5) = {5};

// 5. 结构化约束
Transfinite Curve {1,2,3,4,5,6,7,8} = nC;
Transfinite Curve {9,10,11,12} = nR;

Transfinite Surface {1,2,3,4,5};
Recombine Surface {1,2,3,4,5};

// 6. 扫掠 3D
Extrude {0, 0, H} {
  Surface{1,2,3,4,5};
  Layers{nH};
  Recombine;
}

Mesh 3;
Mesh.MshFileVersion = 2.2;
)")
.arg(radius)
.arg(height)
.arg(nC)
.arg(nH);
}