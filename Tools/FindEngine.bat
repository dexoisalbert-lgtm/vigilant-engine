@echo off
rem Sets UE_ROOT to the Unreal Engine install folder.
rem Override by setting the UE_ROOT environment variable yourself.
set "UE_VERSION=5.7"
if defined UE_ROOT goto :check

for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%UE_VERSION%" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%B"
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_%UE_VERSION%"

:check
if exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" exit /b 0
echo.
echo ERROR: Unreal Engine %UE_VERSION% was not found at:
echo   %UE_ROOT%
echo Install it from the Epic Games Launcher (see Docs\SETUP.md), or set the
echo UE_ROOT environment variable to your engine folder.
exit /b 1
