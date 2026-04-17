cmake_minimum_required(VERSION 3.16)
project(distributed_cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/third_party.cmake)
find_package(Threads REQUIRED)

add_library(cache_server_lib INTERFACE)
target_include_directories(cache_server_lib INTERFACE ${PROJECT_SOURCE_DIR}/src)

add_library(cache_core_lib src/core/concurrent_store.cpp src/core/eviction/slru_lfu_policy.cpp)
target_include_directories(cache_core_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(cache_core_lib PUBLIC Threads::Threads)

add_executable(cache_server src/server/main.cpp)
target_link_libraries(cache_server PRIVATE cache_server_lib)

enable_testing()

add_executable(test_bootstrap tests/smoke/test_bootstrap.cpp)
target_link_libraries(test_bootstrap PRIVATE gtest_main cache_server_lib)
add_test(NAME Bootstrap COMMAND test_bootstrap)

add_executable(test_concurrent_store tests/core/concurrent_store_test.cpp)
target_link_libraries(test_concurrent_store PRIVATE gtest_main cache_core_lib)
add_test(NAME ConcurrentStore COMMAND test_concurrent_store)

add_executable(test_eviction_policy tests/core/eviction_policy_test.cpp)
target_link_libraries(test_eviction_policy PRIVATE gtest_main cache_core_lib)
add_test(NAME EvictionPolicy COMMAND test_eviction_policy)
