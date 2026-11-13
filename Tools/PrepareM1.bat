@echo off
rem Build first. Run with the Unreal Editor closed.
setlocal
set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
call "%~dp0FindEngine.bat"
if errorlevel 1 goto :fail
if not exist "%PROJECT_DIR%\Saved\Logs" mkdir "%PROJECT_DIR%\Saved\Logs"
if exist "%PROJECT_DIR%\Saved\M1Ready.txt" del "%PROJECT_DIR%\Saved\M1Ready.txt"

echo Preparing the M1 swimming tank. Keep the Unreal Editor closed.
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT_DIR%\Paleotrench.uproject" /Engine/Maps/Entry -ExecutePythonScript="%PROJECT_DIR%\Tools\Editor\prepare_m1.py" -unattended -nop4 -nosplash -RenderOffscreen -abslog="%PROJECT_DIR%\Saved\Logs\M1-Prepare.txt"
if errorlevel 1 goto :fail
if not exist "%PROJECT_DIR%\Saved\M1Ready.txt" goto :fail
echo M1 PREPARATION SUCCEEDED. Open Paleotrench.uproject and press Play.
pause
exit /b 0

:fail
echo M1 preparation failed. Run Build.bat first, then try again.
echo If it still fails, run CollectLogs.bat and send the output.
pause
exit /b 1
