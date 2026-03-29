@echo off
::
:: Run Fidelity Tests - Complete validation of campello_widgets against Flutter
::
:: This script:
:: 1. Generates golden files from Flutter (JSON + PNG with REAL fonts)
:: 2. Builds C++ tests
:: 3. Runs C++ fidelity tests (JSON + Visual)
:: 4. Reports results
::

setlocal enabledelayedexpansion

:: Script directory
set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

:: Configuration
set BUILD_DIR=%SCRIPT_DIR%\build\windows
set GOLDENS_DIR=%SCRIPT_DIR%\tests\goldens
set VISUAL_DIR=%SCRIPT_DIR%\tests\visual_fidelity
set FLUTTER_DIR=%SCRIPT_DIR%\flutter_fidelity_tester
set TEST_EXE=%BUILD_DIR%\tests\Release\campello_widgets_tests.exe

:: Defaults
set "SKIP_FLUTTER=false"
set "SKIP_BUILD=false"
set "VERBOSE=false"
set "SPECIFIC_TEST="
set "RUN_VISUAL=true"
set "RUN_JSON=false"

:: Parse arguments
:parse_args
if "%~1"=="" goto :done_parsing
if /i "%~1"=="-h"             goto :usage
if /i "%~1"=="--help"         goto :usage
if /i "%~1"=="--skip-flutter" ( set "SKIP_FLUTTER=true" & shift & goto :parse_args )
if /i "%~1"=="--skip-build"   ( set "SKIP_BUILD=true"  & shift & goto :parse_args )
if /i "%~1"=="--verbose"      ( set "VERBOSE=true"     & shift & goto :parse_args )
if /i "%~1"=="--visual"       ( set "RUN_VISUAL=true"  & set "RUN_JSON=false" & shift & goto :parse_args )
if /i "%~1"=="--json"         ( set "RUN_VISUAL=false" & set "RUN_JSON=true"  & shift & goto :parse_args )
if /i "%~1"=="--all"          ( set "RUN_VISUAL=true"  & set "RUN_JSON=true"  & shift & goto :parse_args )
if /i "%~1"=="--test"         ( set "SPECIFIC_TEST=%~2" & shift & shift & goto :parse_args )
echo [FAIL] Unknown option: %~1
call :print_usage
exit /b 1

:usage
call :print_usage
exit /b 0

:done_parsing

echo ================================================
echo   Campello Widgets - Fidelity Test Suite
echo ================================================
echo.

call :check_prerequisites
if errorlevel 1 exit /b 1

if "%RUN_JSON%"=="true"   call :generate_flutter_json_goldens
if errorlevel 1 exit /b 1
if "%RUN_VISUAL%"=="true" call :generate_flutter_visual_goldens
if errorlevel 1 exit /b 1

call :build_cpp_tests
if errorlevel 1 exit /b 1

set JSON_PASSED=0
set JSON_FAILED=0
set VISUAL_PASSED=0
set VISUAL_FAILED=0

if "%RUN_JSON%"=="true" (
    call :run_json_tests
    set JSON_RESULT=!errorlevel!
)

if "%RUN_VISUAL%"=="true" (
    call :run_visual_tests
    set VISUAL_RESULT=!errorlevel!
)

call :print_summary
exit /b !errorlevel!

:: ============================================================
:print_usage
echo Usage: %~n0 [OPTIONS]
echo.
echo Options:
echo   -h, --help          Show this help message
echo   --skip-flutter      Skip Flutter golden generation (use existing)
echo   --skip-build        Skip C++ build step
echo   --verbose           Verbose output
echo   --test ^<name^>       Run specific test (e.g., SimpleColumn)
echo   --visual            Run visual (PNG) per-pixel tests (default)
echo   --json              Run JSON layout tests only
echo   --all               Run both JSON and visual tests
echo.
echo Examples:
echo   %~n0                  Run visual per-pixel fidelity tests (default)
echo   %~n0 --all            Run both JSON and visual tests
echo   %~n0 --json           Run JSON layout tests only
echo   %~n0 --skip-flutter   Use existing goldens
exit /b 0

:: ============================================================
:check_prerequisites
echo [INFO] Checking prerequisites...

where cmake >nul 2>&1
if errorlevel 1 (
    echo [FAIL] CMake not found. Please install CMake.
    exit /b 1
)

if "%SKIP_FLUTTER%"=="false" (
    where flutter >nul 2>&1
    if errorlevel 1 (
        echo [FAIL] Flutter not found. Install Flutter or use --skip-flutter
        exit /b 1
    )
    for /f "tokens=*" %%v in ('call flutter --version 2^>^&1 ^| findstr /n "." ^| findstr "^1:"') do (
        set FLUTTER_VER=%%v
        set FLUTTER_VER=!FLUTTER_VER:*:=!
    )
    echo [PASS] Flutter found: !FLUTTER_VER!
)

:: CMake locates MSVC via the registry — no need to check for cl.exe in PATH.

echo [PASS] All prerequisites satisfied
echo.
exit /b 0

:: ============================================================
:generate_flutter_json_goldens
if "%SKIP_FLUTTER%"=="true" (
    echo [INFO] Skipping Flutter JSON golden generation ^(--skip-flutter^)
    if not exist "%GOLDENS_DIR%\*_flutter.json" (
        echo [WARN] No existing JSON golden files found in %GOLDENS_DIR%
    ) else (
        echo [PASS] Found existing JSON golden files
    )
    echo.
    exit /b 0
)

echo [INFO] Generating Flutter JSON golden files...

pushd "%FLUTTER_DIR%"

echo [INFO] Getting Flutter dependencies...
if "%VERBOSE%"=="true" (
    call flutter pub get
) else (
    call flutter pub get >nul 2>&1
)

if not exist "%GOLDENS_DIR%" mkdir "%GOLDENS_DIR%"

echo [INFO] Running Flutter tests to generate JSON goldens...
if "%VERBOSE%"=="true" (
    call flutter test test/fidelity_goldens_test.dart
) else (
    call flutter test test/fidelity_goldens_test.dart 2>&1 | findstr /i "Generated: golden"
)

popd

set GOLDEN_COUNT=0
for %%f in ("%GOLDENS_DIR%\*_flutter.json") do set /a GOLDEN_COUNT+=1
if !GOLDEN_COUNT! gtr 0 (
    echo [PASS] Generated !GOLDEN_COUNT! JSON golden files
)
echo.
exit /b 0

:: ============================================================
:download_fonts_if_needed
if not exist "%FLUTTER_DIR%\fonts\Roboto-Regular.ttf" (
    echo [INFO] Fonts not found. Downloading...
    pushd "%FLUTTER_DIR%"
    if exist "download_fonts.sh" (
        bash download_fonts.sh
    ) else (
        echo [WARN] Font download script not found. Text rendering may fail.
    )
    popd
)
exit /b 0

:: ============================================================
:generate_flutter_visual_goldens
if "%SKIP_FLUTTER%"=="true" (
    echo [INFO] Skipping Flutter visual golden generation ^(--skip-flutter^)
    if not exist "%VISUAL_DIR%\flutter_goldens\*.png" (
        echo [WARN] No existing visual golden files found
    ) else (
        echo [PASS] Found existing visual golden files
    )
    echo.
    exit /b 0
)

echo [INFO] Generating Flutter visual golden files (PNG)...

pushd "%FLUTTER_DIR%"

echo [INFO] Getting Flutter dependencies...
if "%VERBOSE%"=="true" (
    call flutter pub get
) else (
    call flutter pub get >nul 2>&1
)

call :download_fonts_if_needed

if not exist "%VISUAL_DIR%\flutter_goldens" mkdir "%VISUAL_DIR%\flutter_goldens"

echo [INFO] Using flutter test for golden generation (Ahem font - colored blocks for text)...
if "%VERBOSE%"=="true" (
    call flutter test test/visual_goldens_test.dart
) else (
    call flutter test test/visual_goldens_test.dart 2>&1 | findstr /i "Generated: Generating passed failed"
)

popd

set GOLDEN_COUNT=0
for %%f in ("%VISUAL_DIR%\flutter_goldens\*.png") do set /a GOLDEN_COUNT+=1
if !GOLDEN_COUNT! gtr 0 (
    echo [PASS] Generated !GOLDEN_COUNT! visual golden files
) else (
    echo [FAIL] No visual golden files were generated
    echo.
    exit /b 1
)
echo.
exit /b 0

:: ============================================================
:build_cpp_tests
if "%SKIP_BUILD%"=="true" (
    echo [INFO] Skipping C++ build ^(--skip-build^)
    if not exist "%TEST_EXE%" (
        echo [FAIL] C++ tests not found. Build first or remove --skip-build
        exit /b 1
    )
    echo [PASS] Using existing build
    echo.
    exit /b 0
)

echo [INFO] Building C++ tests...

echo [INFO] Configuring CMake...
if "%VERBOSE%"=="true" (
    cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DBUILD_TESTS=ON ^
        -DBUILD_EXAMPLES=OFF
) else (
    cmake -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DBUILD_TESTS=ON ^
        -DBUILD_EXAMPLES=OFF >nul 2>&1
)
if errorlevel 1 (
    echo [FAIL] CMake configuration failed
    exit /b 1
)

echo [INFO] Compiling...
if "%VERBOSE%"=="true" (
    cmake --build "%BUILD_DIR%" --target campello_widgets_tests --config Release -- /m:%NUMBER_OF_PROCESSORS%
) else (
    cmake --build "%BUILD_DIR%" --target campello_widgets_tests --config Release -- /m:%NUMBER_OF_PROCESSORS% >nul 2>&1
)
if errorlevel 1 (
    echo [FAIL] Build failed
    exit /b 1
)

if not exist "%TEST_EXE%" (
    echo [FAIL] Build failed - test executable not found: %TEST_EXE%
    exit /b 1
)

echo [PASS] C++ tests built successfully
echo.
exit /b 0

:: ============================================================
:run_json_tests
echo [INFO] Running C++ JSON fidelity tests...
echo.

if defined SPECIFIC_TEST (
    set TEST_FILTER=FlutterGoldenValidation.%SPECIFIC_TEST%
) else (
    set TEST_FILTER=FlutterGoldenValidation*
)

echo ------------------------------------------------
"%TEST_EXE%" --gtest_filter="!TEST_FILTER!"
set TEST_RESULT=!errorlevel!
echo ------------------------------------------------
echo.

if !TEST_RESULT! neq 0 set JSON_FAILED=1
exit /b !TEST_RESULT!

:: ============================================================
:run_visual_tests
echo [INFO] Running C++ visual fidelity tests...
echo.

if defined SPECIFIC_TEST (
    set TEST_FILTER=VisualFidelity.%SPECIFIC_TEST%
) else (
    set TEST_FILTER=VisualFidelity*
)

echo ------------------------------------------------
"%TEST_EXE%" --gtest_filter="!TEST_FILTER!"
set TEST_RESULT=!errorlevel!
echo ------------------------------------------------
echo.

if exist "%VISUAL_DIR%\cpp_output\*.png" (
    echo [INFO] Generated C++ visual output files:
    dir /b "%VISUAL_DIR%\cpp_output\*.png" 2>nul
    echo.
)

if !TEST_RESULT! neq 0 set VISUAL_FAILED=1
exit /b !TEST_RESULT!

:: ============================================================
:print_summary
echo ================================================
echo   Fidelity Test Summary
echo ================================================
echo.

if "%RUN_JSON%"=="true" (
    echo JSON Tests:
    if "%JSON_FAILED%"=="0" (
        echo [PASS]   All JSON tests passed!
    ) else (
        echo [FAIL]   JSON test^(s^) failed
    )
    echo.
)

if "%RUN_VISUAL%"=="true" (
    echo Visual Tests:
    if "%VISUAL_FAILED%"=="0" (
        echo [PASS]   All visual tests passed!
    ) else (
        echo [FAIL]   Visual test^(s^) failed
    )
    echo.
)

if "%RUN_JSON%"=="true"   echo JSON goldens:   %GOLDENS_DIR%
if "%RUN_VISUAL%"=="true" echo Visual goldens: %VISUAL_DIR%\flutter_goldens
if "%RUN_VISUAL%"=="true" echo Visual output:  %VISUAL_DIR%\cpp_output
echo.

if "%JSON_FAILED%"=="1"   exit /b 1
if "%VISUAL_FAILED%"=="1" exit /b 1
exit /b 0
