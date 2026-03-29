set pathToMkexp="C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\mkexp.exe"
rem path to mkexp
set pathToDlls=..\..\bins\Win64\Debug\
rem path to compiled dlls
set pathToAddDlls=..\..\code\SDK\binaries\Win64\
rem path to additional dlls
set pathOutLibs=..\..\libraries\Win64\Debug\

copy ..\..\code\SDK\binaries\Win64\FreeImage.dll %pathToDlls%FreeImage.dll
copy ..\..\code\SDK\binaries\Win64\amd_ags_x64.dll %pathToDlls%amd_ags_x64.dll
copy ..\..\code\SDK\binaries\Win64\D3DX9d_41.dll %pathToDlls%D3DX9d_41.dll
copy ..\..\code\SDK\binaries\Win64\D3DX9d_41.dll %pathToDlls%D3DX9_41.dll

pause
