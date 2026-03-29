set pathToMkexp="C:\Program Files (x86)\Embarcadero\Studio\37.0\bin\mkexp.exe"
rem path to coff2omf
set pathToDlls=bins\Win64\Release\
rem path to compiled dlls
set pathToAddDlls=code\SDK\binaries\Win64\
rem path to additional dlls
set pathOutLibs=libraries\Win64\Release\
rem path to out generated libraries

copy code\SDK\binaries\Win64\FreeImage.dll %pathToDlls%FreeImage.dll
copy code\SDK\binaries\Win64\amd_ags_x64.dll %pathToDlls%amd_ags_x64.dll

copy code\SDK\binaries\Win64\bcbsmp270.bpl %pathToDlls%bcbsmp270.bpl
copy code\SDK\binaries\Win64\borlndmm.dll %pathToDlls%borlndmm.dll
copy code\SDK\binaries\Win64\cc64270.dll %pathToDlls%cc64270.dll
copy code\SDK\binaries\Win64\cc64270mt.dll %pathToDlls%cc64270mt.dll
copy code\SDK\binaries\Win64\rtl270.bpl %pathToDlls%rtl270.bpl
copy code\SDK\binaries\Win64\vcl270.bpl %pathToDlls%vcl270.bpl
copy code\SDK\binaries\Win64\vclimg270.bpl %pathToDlls%vclimg270.bpl
copy code\SDK\binaries\Win64\vclx270.bpl %pathToDlls%vclx270.bpl

pause