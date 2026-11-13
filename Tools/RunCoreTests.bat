@echo off
rem Builds and runs the engine-free core tests with the Visual Studio C++
rem compiler that Unreal already needs (no CMake). Same rules as Unreal:
rem C++20, no exceptions or RTTI, warnings are errors.
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
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1

set "OUT=%ROOT%\Tools\CoreTests\build"
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /std:c++20 /W4 /WX /GR- /EHs-c- /D_HAS_EXCEPTIONS=0 /O2 ^
	/I "%ROOT%\Source\PaleotrenchCore\Public" ^
	"%ROOT%\Source\PaleotrenchCore\Private\*.cpp" "%ROOT%\Tools\CoreTests\*.cpp" ^
	/Fo"%OUT%\\" /Fe"%OUT%\core_tests.exe"
if errorlevel 1 (
	echo CORE TEST BUILD FAILED
	exit /b 1
)
"%OUT%\core_tests.exe"
