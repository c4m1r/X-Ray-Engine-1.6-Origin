@echo off
setlocal
cd /d "%~dp0"

where py >nul 2>&1
if errorlevel 1 goto try_python

py -3 "%~dp0build.py" --config Debug %*
set EXITCODE=%ERRORLEVEL%
goto end_pause

:try_python
where python >nul 2>&1
if errorlevel 1 goto no_python

python "%~dp0build.py" --config Debug %*
set EXITCODE=%ERRORLEVEL%
goto end_pause

:no_python
echo ERROR: Python not found. Install Python 3 or use the `py` launcher.
set EXITCODE=2

:end_pause
echo.
if not "%EXITCODE%"=="0" echo *** BUILD FAILED ***
echo Exit code: %EXITCODE%
echo.
pause
exit /b %EXITCODE%
