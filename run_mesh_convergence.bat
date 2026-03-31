@echo off
set DYNA_PATH="ls-dyna_smp_d_R13.exe"

echo Running Mesh Convergence Step 1 (Size: 5 mm)
%DYNA_PATH% I=MeshConv_Step1_Size5.k NCPU=4 MEMORY=2000m

echo Running Mesh Convergence Step 2 (Size: 2.5 mm)
%DYNA_PATH% I=MeshConv_Step2_Size2.5.k NCPU=4 MEMORY=2000m

echo Running Mesh Convergence Step 3 (Size: 1.25 mm)
%DYNA_PATH% I=MeshConv_Step3_Size1.25.k NCPU=4 MEMORY=2000m

echo All Mesh Convergence Jobs Finished!
pause
