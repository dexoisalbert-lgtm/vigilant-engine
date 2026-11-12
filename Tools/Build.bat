@echo off
rem Compiles the Paleotrench editor build. Double-click to run.
setlocal
set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
set "PROJECT=%PROJECT_DIR%\Paleotrench.uproject"
set "BUILD_LOG=%PROJECT_DIR%\Saved\Logs\UBT-Build.txt"

call "%~dp0FindEngine.bat"
if errorlevel 1 goto :fail

if not exist "%PROJECT_DIR%\Saved\Logs" mkdir "%PROJECT_DIR%\Saved\Logs"

echo Building Paleotrench with "%UE_ROOT%"
echo The first build takes several minutes.
echo.
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" PaleotrenchEditor Win64 Development -Project="%PROJECT%" -WaitMutex -Log="%BUILD_LOG%"
if errorlevel 1 goto :fail

echo.
echo ==== BUILD SUCCEEDED ====
echo You can now open Paleotrench.uproject.
pause
exit /b 0

:fail
echo.
echo ==== BUILD FAILED ====
echo Run Tools\CollectLogs.bat and paste the result to Claude.
pause
exit /b 1
