@echo off
setlocal

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"

set "pathToDlls=%ROOT%\bins\Win64\Release"
rem path to compiled dlls

copy /Y "%ROOT%\code\SDK\binaries\Win64\FreeImage.dll" "%pathToDlls%\FreeImage.dll"
copy /Y "%ROOT%\code\SDK\binaries\Win64\amd_ags_x64.dll" "%pathToDlls%\amd_ags_x64.dll"

copy /Y "%ROOT%\code\SDK\binaries\Win64\D3DX9_41.dll" "%pathToDlls%\D3DX9_41.dll"

copy /Y "%ROOT%\code\SDK\binaries\Win64\bcbsmpx370.bpl" "%pathToDlls%\bcbsmpx370.bpl"
copy /Y "%ROOT%\code\SDK\binaries\Win64\libc++-370.dll" "%pathToDlls%\libc++-370.dll"
copy /Y "%ROOT%\code\SDK\binaries\Win64\libunwind-370.dll" "%pathToDlls%\libunwind-370.dll"
copy /Y "%ROOT%\code\SDK\binaries\Win64\rtl370.bpl" "%pathToDlls%\rtl370.bpl"
copy /Y "%ROOT%\code\SDK\binaries\Win64\vcl370.bpl" "%pathToDlls%\vcl370.bpl"
copy /Y "%ROOT%\code\SDK\binaries\Win64\vclimg370.bpl" "%pathToDlls%\vclimg370.bpl"
copy /Y "%ROOT%\code\SDK\binaries\Win64\vclx370.bpl" "%pathToDlls%\vclx370.bpl"

copy /Y "%ROOT%\code\SDK\libraries\Win64\nvtt30205.dll" "%pathToDlls%\nvtt30205.dll"
copy /Y "%ROOT%\code\SDK\libraries\Win64\cudart64_13.dll" "%pathToDlls%\cudart64_13.dll"
copy /Y "%ROOT%\code\SDK\libraries\Win64\vcomp140.dll" "%pathToDlls%\vcomp140.dll"
copy /Y "%ROOT%\code\SDK\libraries\Win64\FreeImage.dll" "%pathToDlls%\FreeImage.dll"

if /I not "%~1"=="--no-pause" pause