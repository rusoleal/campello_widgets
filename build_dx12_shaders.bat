@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "SHADER_DIR=%SCRIPT_DIR%\shaders\dx12"
set "OUT_HEADER=%SCRIPT_DIR%\src\shaders\dx12_widgets.h"

if not exist "%SCRIPT_DIR%\src\shaders" mkdir "%SCRIPT_DIR%\src\shaders"

:: ---------------------------------------------------------------------
:: Locate fxc.exe (Windows SDK HLSL compiler) — picks the newest SDK.
:: ---------------------------------------------------------------------
set "SDK_BIN=C:\Program Files (x86)\Windows Kits\10\bin"
set "FXC="
for /f "delims=" %%V in ('dir /b /o-n "%SDK_BIN%"') do (
    if not defined FXC (
        if exist "%SDK_BIN%\%%V\x64\fxc.exe" set "FXC=%SDK_BIN%\%%V\x64\fxc.exe"
    )
)
if not defined FXC (
    echo [build_dx12_shaders] Could not find fxc.exe under Windows Kits.
    exit /b 1
)
echo Using fxc: %FXC%

:: ---------------------------------------------------------------------
:: Compile HLSL -> DXBC (.cso)
:: ---------------------------------------------------------------------
call :compile rect.hlsl  RectVS  vs_5_0 rect_vs.cso
call :compile rect.hlsl  RectPS  ps_5_0 rect_ps.cso
call :compile rect_aa.hlsl RectAAVS vs_5_0 rect_aa_vs.cso
call :compile rect_aa.hlsl RectAAPS ps_5_0 rect_aa_ps.cso
call :compile vertices.hlsl VerticesVS vs_5_0 vertices_vs.cso
call :compile vertices.hlsl VerticesPS ps_5_0 vertices_ps.cso
:: quad/blur/clip_shape/shader_mask/icon declare their second bind group's
:: t/s registers with an explicit spaceN (see quad.hlsl's doc comment for
:: why) — X3721 "space is only supported for shader targets 5.1 and higher",
:: so these five need vs_5_1/ps_5_1 specifically; everything else stays on
:: 5_0 since a single-bind-group shader has nothing that needs it.
call :compile quad.hlsl  QuadVS  vs_5_1 quad_vs.cso
call :compile quad.hlsl  QuadPS  ps_5_1 quad_ps.cso
call :compile shape.hlsl ShapeVS vs_5_0 shape_vs.cso
call :compile shape.hlsl ShapePS ps_5_0 shape_ps.cso
call :compile arc.hlsl   ArcVS   vs_5_0 arc_vs.cso
call :compile arc.hlsl   ArcPS   ps_5_0 arc_ps.cso
call :compile line.hlsl  LineVS  vs_5_0 line_vs.cso
call :compile line.hlsl  LinePS  ps_5_0 line_ps.cso
call :compile blur.hlsl  BlurVS  vs_5_1 blur_vs.cso
call :compile blur.hlsl  BlurPS  ps_5_1 blur_ps.cso
call :compile clip_shape.hlsl ClipShapeVS vs_5_1 clip_shape_vs.cso
call :compile clip_shape.hlsl ClipShapePS ps_5_1 clip_shape_ps.cso
call :compile shader_mask.hlsl ShaderMaskVS vs_5_1 shader_mask_vs.cso
call :compile shader_mask.hlsl ShaderMaskPS ps_5_1 shader_mask_ps.cso
call :compile icon.hlsl  IconVS  vs_5_1 icon_vs.cso
call :compile icon.hlsl  IconPS  ps_5_1 icon_ps.cso
if errorlevel 1 exit /b 1

:: ---------------------------------------------------------------------
:: Generate embedded C++ header from the compiled .cso files
:: ---------------------------------------------------------------------
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%\build_dx12_shaders.ps1" ^
    -ShaderDir "%SHADER_DIR%" -OutHeader "%OUT_HEADER%"
if errorlevel 1 exit /b 1

echo Written: %OUT_HEADER%
exit /b 0

:: ---------------------------------------------------------------------
:compile
:: %1=source .hlsl  %2=entry point  %3=profile  %4=output .cso
"%FXC%" /nologo /T %3 /E %2 /Fo "%SHADER_DIR%\%4" "%SHADER_DIR%\%1"
exit /b %errorlevel%
