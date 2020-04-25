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

clang-tidy src\cli.cpp src\cli.hpp src\common.hpp src\compressor.hpp src\constants.hpp ^
           src\consumer.cpp src\consumer.hpp src\deref_proxy.hpp src\exception.hpp src\filecache.cpp ^
           src\filecache.hpp src\format.hpp src\main.cpp src\network.cpp  src\network.hpp src\os.cpp ^
           src\os.hpp src\os_win32.cpp src\reader.cpp src\reader.hpp src\stringcache.hpp ^
        -- -DBOOST_ALL_NO_LIB -DXXH_INLINE_ALL -DNTDDI_VERSION=NTDDI_WIN7 -D_WIN32_WINNT=_WIN32_WINNT_WIN7 ^
           -DUNICODE -D_UNICODE -D_WINSOCK_DEPRECATED_NO_WARNINGS ^
           -Iext\readerwriterqueue -Iext\lz4\lib -Iext\xxhash -isystem C:\dep_libs\include || exit /b 1
