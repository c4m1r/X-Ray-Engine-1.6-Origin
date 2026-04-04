@echo off
setlocal

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"

set "pathToDlls=%ROOT%\bins\Win64\Release"
rem path to compiled dlls

copy /Y "%ROOT%\code\SDK\binaries\Win64\FreeImage.dll" "%pathToDlls%\FreeImage.dll"
copy /Y "%ROOT%\code\SDK\binaries\Win64\amd_ags_x64.dll" "%pathToDlls%\amd_ags_x64.dll"

if /I not "%~1"=="--no-pause" pause