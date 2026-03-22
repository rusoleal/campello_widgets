@echo off
setlocal

set BUILD_DIR=build\windows

cmake -S . -B %BUILD_DIR% ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_EXAMPLES=ON

cmake --build %BUILD_DIR% --config Release
