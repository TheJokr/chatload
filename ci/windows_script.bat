@echo off

REM reset predefined compiler variables and load MSVC ones
set CC=cl
set CC_FOR_BUILD=cl
set CXX=cl
set CXX_FOR_BUILD=cl
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\Common7\Tools\VsDevCmd.bat" ^
       -host_arch=amd64 -arch=amd64 || exit /b 1

set CMAKE_PREFIX_PATH=C:\dep_libs
mkdir build
cmake -G Ninja -B build || exit /b 1

cmake --build build -j 2 || exit /b 1
.\build\chatload -hV || exit /b 1
