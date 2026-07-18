@echo off
setlocal

set "MODE=%~1"
if "%MODE%"=="" set "MODE=debug"

if /i "%MODE%"=="debug" (
    set "BUILD_TYPE=Debug"
) else if /i "%MODE%"=="release" (
    set "BUILD_TYPE=Release"
) else (
    echo Usage: %~n0 [debug^|release]
    exit /b 1
)

set "SCRIPT_DIR=%~dp0"
set "ROOT=%SCRIPT_DIR%..\..\.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "BUILD_DIR=%ROOT%\build\windows-%MODE%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not defined INCLUDE goto :setup_vcvars
goto :vcvars_done

:setup_vcvars
for /f "usebackq tokens=*" %%V in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VS_PATH=%%V"
if not defined VS_PATH (
    echo [run.bat] Could not locate a Visual Studio installation via vswhere.
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1

:vcvars_done

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    cmake -S "%ROOT%" -B "%BUILD_DIR%" ^
        -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
        -DBUILD_EXAMPLES=ON
    if errorlevel 1 exit /b 1
)

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --target campello_widgets_gallery
if errorlevel 1 exit /b 1

:: Single-config generators (Ninja) place the exe directly in BUILD_DIR;
:: multi-config generators (Visual Studio) nest it under a %BUILD_TYPE%
:: subfolder - check both rather than assuming one generator.
set "EXE=%BUILD_DIR%\campello_widgets_gallery.exe"
if not exist "%EXE%" set "EXE=%BUILD_DIR%\%BUILD_TYPE%\campello_widgets_gallery.exe"
if not exist "%EXE%" (
    echo [run.bat] Could not find campello_widgets_gallery.exe under %BUILD_DIR%.
    exit /b 1
)

"%EXE%"
