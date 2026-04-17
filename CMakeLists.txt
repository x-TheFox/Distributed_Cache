cmake_minimum_required(VERSION 3.16)
project(distributed_cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/third_party.cmake)

add_library(cache_server_lib INTERFACE)
target_include_directories(cache_server_lib INTERFACE ${PROJECT_SOURCE_DIR}/src)

add_executable(cache_server src/server/main.cpp)
target_link_libraries(cache_server PRIVATE cache_server_lib)

enable_testing()

add_executable(test_bootstrap tests/smoke/test_bootstrap.cpp)
target_link_libraries(test_bootstrap PRIVATE gtest_main cache_server_lib)
add_test(NAME Bootstrap COMMAND test_bootstrap)
