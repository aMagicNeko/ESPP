cmake_minimum_required(VERSION 3.14)
project(MEVpp VERSION 1.0 DESCRIPTION "CPP MEV")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(Boost_NO_BOOST_CMAKE ON)

#add_compile_options(-g -O2 -DNDEBUG -pipe -Wall -Werror -fPIC -Wno-deprecated -Wno-unused-parameter -fno-omit-frame-pointer -D__STDC_FORMAT_MACROS -DBAIDU_RPC_ENABLE_CPU_PROFILER -DBAIDU_RPC_ENABLE_HEAP_PROFILER -lz)
add_compile_options(-g -pipe -Wall -Werror -fPIC -Wno-deprecated -Wno-narrowing -Wno-unused-parameter -fno-omit-frame-pointer -D__STDC_FORMAT_MACROS -DBAIDU_RPC_ENABLE_CPU_PROFILER -DBAIDU_RPC_ENABLE_HEAP_PROFILER -lz -Wno-sign-compare -Wno-unknown-pragmas -Wno-ignored-attributes -Wno-unused-function)

find_package(protobuf CONFIG REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(gflags CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(cryptopp REQUIRED)
find_package(evmc REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)
find_package(leveldb REQUIRED)

set(SECP256K1_LIBRARY "/usr/local/lib/libsecp256k1.a")

find_library(SCRYPT_LIBRARY scrypt)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/util/Aleth)
set(MY_LIBS cryptopp evmone brpc gflags OpenSSL::SSL OpenSSL::Crypto Boost::system Boost::filesystem protobuf::libprotobuf protobuf::libprotoc protobuf::libprotobuf-lite ZLIB::ZLIB leveldb ${SECP256K1_LIBRARY} ${SCRYPT_LIBRARY})

link_libraries(pthread rt)

file(GLOB UTIL_SOURCES "util/*.cpp")
file(GLOB ALETH_SOURCES "util/Aleth/*.cpp"
                        "util/Aleth/ethash/*.cpp"
                        "util/Aleth/ethash/*.cpp"
                        "util/Aleth/libdevcrypto/*.cpp"
                        "util/Aleth/libethcore/*.cpp")
file(GLOB DATA_SOURCES "data/*.cpp")
file(GLOB SIMULATE_SOURCES "simulate/*.cpp")
file(GLOB SEARCH_SOURCES "search/*.cpp")
file(GLOB GATEWAY_SOURCES "gateway/*.cpp")
add_executable(test_websocket test/test_websocket.cpp ${UTIL_SOURCES} ${DATA_SOURCES} ${SIMULATE_SOURCES} ${SEARCH_SOURCES} ${GATEWAY_SOURCES} ${ALETH_SOURCES})
add_executable(test_abi test/test_abi.cpp ${UTIL_SOURCES} ${DATA_SOURCES} ${SIMULATE_SOURCES} ${SEARCH_SOURCES} ${GATEWAY_SOURCES} ${ALETH_SOURCES})
add_executable(test_simulate test/test_simulate.cpp ${UTIL_SOURCES} ${DATA_SOURCES} ${SIMULATE_SOURCES} ${SEARCH_SOURCES} ${GATEWAY_SOURCES} ${ALETH_SOURCES})
add_executable(test_pool test/test_pool.cpp ${UTIL_SOURCES} ${DATA_SOURCES} ${SIMULATE_SOURCES} ${SEARCH_SOURCES} ${GATEWAY_SOURCES} ${ALETH_SOURCES})
add_executable(test_bundle test/test_bundle.cpp ${UTIL_SOURCES} ${DATA_SOURCES} ${SIMULATE_SOURCES} ${SEARCH_SOURCES} ${GATEWAY_SOURCES} ${ALETH_SOURCES})

add_executable(foo foo.cpp)

target_link_libraries(test_websocket PRIVATE ${MY_LIBS} dl)
target_link_libraries(test_abi PRIVATE ${MY_LIBS} dl)
target_link_libraries(test_simulate PRIVATE ${MY_LIBS} dl)
target_link_libraries(test_pool PRIVATE ${MY_LIBS} dl)
target_link_libraries(test_bundle PRIVATE ${MY_LIBS} dl)

target_link_libraries(foo PRIVATE ${MY_LIBS} dl)
