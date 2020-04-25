@echo off

if not "%TRAVIS_OS_NAME%"=="windows" (
    echo Not running on windows CI environment. Exiting.
    exit /b
)

if exist C:\dep_libs\lib (
    echo Cached dependencies detected. Exiting.
    exit /b
)

REM required to compile OpenSSL
choco install nasm strawberryperl || exit /b 1
call refreshenv.cmd || exit /b 1

REM add NASM to PATH manually
set PATH=%PATH%;C:\Program Files\NASM

wget -nv https://dl.bintray.com/boostorg/release/1.72.0/source/boost_1_72_0.7z ^
         https://www.openssl.org/source/openssl-1.1.1g.tar.gz || exit /b 1
7z x boost_1_72_0.7z || exit /b 1
7z x openssl-1.1.1g.tar.gz || exit /b 1
7z x openssl-1.1.1g.tar || exit /b 1

REM reset predefined compiler variables and load MSVC ones
set CC=cl
set CC_FOR_BUILD=cl
set CXX=cl
set CXX_FOR_BUILD=cl
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\Common7\Tools\VsDevCmd.bat" ^
       -host_arch=amd64 -arch=amd64 || exit /b 1

cd boost_1_72_0
call bootstrap.bat || exit /b 1
.\b2 --prefix=C:\dep_libs -j 2 -d0 --with-program_options variant=debug,release ^
     link=static threading=multi runtime-link=static address-model=64 install || exit /b 1
cd ..

cd openssl-1.1.1g
perl Configure VC-WIN64A --prefix=C:\dep_libs no-deprecated no-shared no-tests || exit /b 1
nmake /NOLOGO /S || exit /b 1
nmake /NOLOGO /S install || exit /b 1
cd ..

del /q boost_1_72_0.7z openssl-1.1.1g.tar.gz openssl-1.1.1g.tar
rmdir /q /s boost_1_72_0 openssl-1.1.1g
