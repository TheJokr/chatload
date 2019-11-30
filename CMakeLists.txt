# CMake 3.8 added cxx_std_* features
cmake_minimum_required(VERSION 3.8)

project(chatload-client VERSION 2.0.0 LANGUAGES C CXX)
# Disable C++ vendor extensions by default
set(CMAKE_CXX_EXTENSIONS OFF)
# Use static runtime with MSVC by default
if (MSVC)
    foreach(lang C CXX)
        foreach(config DEBUG MINSIZEREL RELEASE RELWITHDEBINFO)
            string(REPLACE "/MD" "/MT" CMAKE_${lang}_FLAGS_${config} "${CMAKE_${lang}_FLAGS_${config}}")
        endforeach(config)
    endforeach(lang)
endif()

# Boost
set(Boost_USE_STATIC_LIBS ON)
if (MSVC)
    set(Boost_USE_STATIC_RUNTIME ON)
endif()
find_package(Boost 1.69 REQUIRED COMPONENTS program_options)

# OpenSSL (for Boost.Asio)
if (WIN32)
    set(OPENSSL_USE_STATIC_LIBS ON)
endif()
find_package(OpenSSL REQUIRED)

# LZ4 (with framing)
add_library(lz4 STATIC EXCLUDE_FROM_ALL "ext/lz4/lib/lz4.c" "ext/lz4/lib/lz4hc.c"
                "ext/lz4/lib/lz4frame.c" "ext/lz4/lib/xxhash.c")
target_compile_definitions(lz4 PRIVATE "XXH_NAMESPACE=LZ4_")
target_include_directories(lz4 INTERFACE "ext/lz4/lib")

# xxHash (inlined)
add_library(xxhash INTERFACE)
target_compile_definitions(xxhash INTERFACE "XXH_INLINE_ALL")
target_include_directories(xxhash INTERFACE "ext/xxhash")

add_executable(chatload "src/main.cpp" "src/cli.cpp" "src/reader.cpp" "src/os.cpp"
                   "src/filecache.cpp" "src/consumer.cpp" "src/network.cpp")
if (WIN32)
    target_sources(chatload PRIVATE "src/os_win32.cpp")
elseif (APPLE)
    target_sources(chatload PRIVATE "src/os_darwin.cpp")
endif()

target_compile_features(chatload PRIVATE cxx_std_14)
if (MSVC)
    target_compile_options(chatload PRIVATE "/W4" "/WX" "/utf-8" "/permissive-")
elseif (UNIX)
    target_compile_options(chatload PRIVATE "-Wall" "-Wextra" "-pedantic" "-Werror")
endif()

target_include_directories(chatload PRIVATE "ext/readerwriterqueue")
target_link_libraries(chatload PRIVATE Boost::disable_autolinking Boost::boost Boost::program_options)
target_link_libraries(chatload PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(chatload PRIVATE lz4)
target_link_libraries(chatload PRIVATE xxhash)
