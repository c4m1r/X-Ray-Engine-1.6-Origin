@echo off
REM Post-build: pack editors\images\Splash\*.jpg -> SplashImages.db_e next to target exe.
if "%~1"=="" exit /b 0
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0pack_editor_splash.ps1" -OutputDir "%~1"
exit /b 0
