cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE /root/vcpkg/scripts/buildsystems/vcpkg.cmake)
project(MEVpp VERSION 1.0 DESCRIPTION "CPP MEV")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-g -O2 -DNDEBUG -pipe -Wall -Werror -fPIC -Wno-deprecated -Wno-unused-parameter -fno-omit-frame-pointer -D__STDC_FORMAT_MACROS -DBAIDU_RPC_ENABLE_CPU_PROFILER -DBAIDU_RPC_ENABLE_HEAP_PROFILER -lz)

link_directories(/usr/lib/aarch64-linux-gnu/libssl.a)

find_package(protobuf CONFIG REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(gflags CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cryptopp REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(/root/vcpkg/installed/arm64-linux/include)


message(STATUS "ZLIB_FOUND: ${ZLIB_FOUND}")
message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")

set(MY_LIBS cryptopp evmone brpc gflags OpenSSL::SSL OpenSSL::Crypto Boost::system protobuf::libprotobuf protobuf::libprotoc protobuf::libprotobuf-lite ZLIB::ZLIB)

link_libraries(pthread rt)

file(GLOB UTIL_SOURCES "util/*.cpp")
file(GLOB DATA_SOURCES "data/*.cpp")
add_executable(test_websocket test/test_websocket.cpp ${UTIL_SOURCES} ${DATA_SOURCES})
add_executable(test_abi test/test_abi.cpp ${UTIL_SOURCES} ${DATA_SOURCES})
target_link_libraries(test_websocket PRIVATE ${MY_LIBS} dl)
target_link_libraries(test_abi PRIVATE ${MY_LIBS} dl)
