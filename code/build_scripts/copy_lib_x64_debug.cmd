set pathToDlls=..\..\bins\Win64\Debug\
rem path to compiled dlls

copy ..\..\code\SDK\binaries\Win64\FreeImage.dll %pathToDlls%FreeImage.dll
copy ..\..\code\SDK\binaries\Win64\amd_ags_x64.dll %pathToDlls%amd_ags_x64.dll
copy ..\..\code\SDK\binaries\Win64\D3DX9d_41.dll %pathToDlls%D3DX9d_41.dll
copy ..\..\code\SDK\binaries\Win64\D3DX9d_41.dll %pathToDlls%D3DX9_41.dll

pause
