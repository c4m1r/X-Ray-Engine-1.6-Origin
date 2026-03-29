set pathToMkexp="C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\mkexp.exe"
rem path to mkexp
set pathToDlls=..\..\bins\Win64\Debug\
rem path to compiled dlls
set pathToAddDlls=..\..\code\SDK\binaries\Win64\
rem path to additional dlls
set pathOutLibs=..\..\libraries\Win64\Debug\
%pathToMkexp% %pathOutLibs%d3dx9d_41.lib %pathToAddDlls%D3DX9d_41.dll
copy ..\..\code\SDK\binaries\Win64\FreeImage.dll %pathToDlls%FreeImage.dll
copy ..\..\code\SDK\binaries\Win64\amd_ags_x64.dll %pathToDlls%amd_ags_x64.dll
copy ..\..\code\SDK\binaries\Win64\D3DX9d_41.dll %pathToDlls%D3DX9d_41.dll

REM copy ..\..\code\SDK\binaries\Win64\bcbsmp270.bpl %pathToDlls%bcbsmp270.bpl
REM copy ..\..\code\SDK\binaries\Win64\borlndmm.dll %pathToDlls%borlndmm.dll
REM copy ..\..\code\SDK\binaries\Win64\cc64270.dll %pathToDlls%cc64270.dll
REM copy ..\..\code\SDK\binaries\Win64\cc64270mt.dll %pathToDlls%cc64270mt.dll
REM copy ..\..\code\SDK\binaries\Win64\rtl270.bpl %pathToDlls%rtl270.bpl
REM copy ..\..\code\SDK\binaries\Win64\vcl270.bpl %pathToDlls%vcl270.bpl
REM copy ..\..\code\SDK\binaries\Win64\vclimg270.bpl %pathToDlls%vclimg270.bpl
REM copy ..\..\code\SDK\binaries\Win64\vclx270.bpl %pathToDlls%vclx270.bpl

REM copy ..\..\code\SDK\binaries\Win64\msvcp140d.dll %pathToDlls%msvcp140d.dll
REM copy ..\..\code\SDK\binaries\Win64\vcruntime140d.dll %pathToDlls%vcruntime140d.dll
REM copy ..\..\code\SDK\binaries\Win64\vcruntime140_1d.dll %pathToDlls%vcruntime140_1d.dll

pause
