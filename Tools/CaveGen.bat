@echo off
rem Builds and runs cavegen: prints the layout of any cave seed (see Tools\CaveGen\CaveGenMain.cpp).
rem Example: Tools\CaveGen.bat 0x1234abcd --svg cave.svg
setlocal
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo ERROR: Visual Studio was not found. See Docs\SETUP.md step 1.
	exit /b 1
)
set "VSDIR="
for /f "usebackq delims=" %%P in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%P"
if not defined VSDIR (
	echo ERROR: the Visual Studio C++ tools are missing. See Docs\SETUP.md step 1.
	exit /b 1
)
set "OUT=%ROOT%\Tools\CaveGen\build"
if not exist "%OUT%" mkdir "%OUT%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /W4 /WX /GR- /EHs-c- /D_HAS_EXCEPTIONS=0 /O2 ^
	/I "%ROOT%\Source\PaleotrenchCore\Public" ^
	"%ROOT%\Source\PaleotrenchCore\Private\*.cpp" "%ROOT%\Tools\CaveGen\*.cpp" ^
	/Fo"%OUT%\\" /Fe"%OUT%\cavegen.exe" >"%OUT%\build.log"
if errorlevel 1 (
	type "%OUT%\build.log"
	echo CAVEGEN BUILD FAILED
	exit /b 1
)
"%OUT%\cavegen.exe" %* --tuning "%ROOT%\Data\CaveTuning.csv"
