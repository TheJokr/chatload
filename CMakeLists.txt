# CMake 3.8 added cxx_std_* features
# CMake 3.9 added CheckIPOSupported
cmake_minimum_required(VERSION 3.9)

project(chatload-client VERSION 2.0.0 LANGUAGES C CXX)
# Disable C++ vendor extensions by default
set(CMAKE_CXX_EXTENSIONS OFF)
# Enable compile command database for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Use static runtime with MSVC by default
if (MSVC)
    foreach(lang C CXX)
        foreach(config DEBUG MINSIZEREL RELEASE RELWITHDEBINFO)
            string(REPLACE "/MD" "/MT" CMAKE_${lang}_FLAGS_${config} "${CMAKE_${lang}_FLAGS_${config}}")
        endforeach(config)
    endforeach(lang)
endif()

# Check IPO support for non-DEBUG builds
if (CMAKE_BUILD_TYPE STREQUAL DEBUG)
    set(CHATLOAD_USE_IPO OFF)
else()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT CHATLOAD_USE_IPO LANGUAGES CXX)
endif()


# Boost
set(Boost_USE_STATIC_LIBS ON)
if (MSVC)
    set(Boost_USE_STATIC_RUNTIME ON)
endif()
find_package(Boost 1.71 REQUIRED COMPONENTS program_options)

# OpenSSL (for Boost.Asio)
if (WIN32)
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()
find_package(OpenSSL 1.1.0 REQUIRED)


# readerwriterqueue
add_library(readerwriterqueue INTERFACE)
target_include_directories(readerwriterqueue INTERFACE "ext/readerwriterqueue")

# LZ4 (with framing)
add_library(lz4 STATIC EXCLUDE_FROM_ALL "ext/lz4/lib/lz4.c" "ext/lz4/lib/lz4hc.c"
                "ext/lz4/lib/lz4frame.c" "ext/lz4/lib/xxhash.c")
target_compile_definitions(lz4 PRIVATE "XXH_NAMESPACE=LZ4_")
target_compile_definitions(lz4 INTERFACE "LZ4F_STATIC_LINKING_ONLY")
target_include_directories(lz4 INTERFACE "ext/lz4/lib")

# xxHash (inlined)
add_library(xxhash INTERFACE)
target_compile_definitions(xxhash INTERFACE "XXH_INLINE_ALL")
target_include_directories(xxhash INTERFACE "ext/xxhash")

# robin-hood-hashing
add_subdirectory("ext/robin-hood-hashing" EXCLUDE_FROM_ALL)


# chatload
add_executable(chatload "src/main.cpp" "src/cli.cpp" "src/reader.cpp" "src/os.cpp"
                   "src/filecache.cpp" "src/consumer.cpp" "src/network.cpp" "src/logparser.cpp")
if (WIN32)
    target_sources(chatload PRIVATE "src/os_win32.cpp")
    target_compile_definitions(chatload PRIVATE "NTDDI_VERSION=NTDDI_WIN7" "_WIN32_WINNT=_WIN32_WINNT_WIN7"
                                        "_WINSOCK_DEPRECATED_NO_WARNINGS" "UNICODE" "_UNICODE")
    target_link_libraries(chatload PRIVATE "ws2_32" "gdi32" "advapi32" "crypt32" "user32" "ole32" "shell32" "kernel32")
elseif (APPLE)
    target_sources(chatload PRIVATE "src/os_darwin.cpp")
    target_link_libraries(chatload PRIVATE "-framework CoreFoundation" "-framework Security")
endif()

target_compile_features(chatload PRIVATE cxx_std_14)
set_target_properties(chatload PROPERTIES INTERPROCEDURAL_OPTIMIZATION "${CHATLOAD_USE_IPO}")
if (MSVC)
    target_compile_options(chatload PRIVATE "/W4" "/WX" "/utf-8" "/permissive-")
elseif (UNIX)
    target_compile_options(chatload PRIVATE "-Wall" "-Wextra" "-pedantic" "-Werror")
endif()

target_link_libraries(chatload PRIVATE Boost::disable_autolinking Boost::boost Boost::program_options)
target_link_libraries(chatload PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(chatload PRIVATE readerwriterqueue)
target_link_libraries(chatload PRIVATE lz4)
target_link_libraries(chatload PRIVATE xxhash)
target_link_libraries(chatload PRIVATE robin_hood)


# Custom target to generate chatload build information
add_custom_target(
    chatload-buildinfo "${CMAKE_COMMAND}" "-D" "SRC=${CMAKE_CURRENT_SOURCE_DIR}/src/buildinfo.hpp.in"
      "-D" "DST=${CMAKE_CURRENT_SOURCE_DIR}/src/buildinfo.hpp"
      "-D" "BUILD_VERSION=v${PROJECT_VERSION}"
      "-D" "CACHE_FILE=${CMAKE_CURRENT_BINARY_DIR}/chatload-buildinfo.txt"
      "-P" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/configure_buildinfo.cmake"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/buildinfo.hpp.in"
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/src/buildinfo.hpp"
    COMMENT "Updating buildinfo.hpp"
)
add_dependencies(chatload chatload-buildinfo)
