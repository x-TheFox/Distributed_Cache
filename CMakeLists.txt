cmake_minimum_required(VERSION 3.16)
project(distributed_cache LANGUAGES CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_SKIP_INSTALL_RULES ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/third_party.cmake)

set(CMAKE_CXX_STANDARD 20)
find_package(Threads REQUIRED)

add_library(cache_server_lib INTERFACE)
target_include_directories(cache_server_lib INTERFACE ${PROJECT_SOURCE_DIR}/src)

add_library(cache_core_lib src/core/concurrent_store.cpp
                           src/core/eviction/slru_lfu_policy.cpp
                           src/core/request_coalescer.cpp)
target_include_directories(cache_core_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(cache_core_lib PUBLIC Threads::Threads)

add_library(cache_protocol_lib src/protocol/resp/resp_parser.cpp
                               src/server/command_dispatcher.cpp)
target_include_directories(cache_protocol_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(cache_protocol_lib PUBLIC cache_core_lib)

add_library(cache_cluster_lib src/cluster/hash_ring.cpp
                              src/cluster/shard_router.cpp)
target_include_directories(cache_cluster_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)

add_library(cache_replication_lib src/replication/wal_writer.cpp
                                  src/replication/replica_stream.cpp)
target_include_directories(cache_replication_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)

add_library(cache_control_lib src/control/heartbeat_manager.cpp
                              src/control/raft_metadata_adapter.cpp)
target_include_directories(cache_control_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)

add_library(cache_metrics_lib src/metrics/prometheus_endpoint.cpp)
target_include_directories(cache_metrics_lib PUBLIC ${PROJECT_SOURCE_DIR}/src)

set(CACHE_PROTO ${PROJECT_SOURCE_DIR}/proto/cache.proto)
get_filename_component(CACHE_PROTO_PATH ${CACHE_PROTO} PATH)
set(CACHE_PROTO_SRCS ${CMAKE_CURRENT_BINARY_DIR}/cache.pb.cc)
set(CACHE_PROTO_HDRS ${CMAKE_CURRENT_BINARY_DIR}/cache.pb.h)
set(CACHE_GRPC_SRCS ${CMAKE_CURRENT_BINARY_DIR}/cache.grpc.pb.cc)
set(CACHE_GRPC_HDRS ${CMAKE_CURRENT_BINARY_DIR}/cache.grpc.pb.h)

add_custom_command(
  OUTPUT ${CACHE_PROTO_SRCS} ${CACHE_PROTO_HDRS} ${CACHE_GRPC_SRCS}
         ${CACHE_GRPC_HDRS}
  COMMAND $<TARGET_FILE:protoc>
  ARGS --grpc_out ${CMAKE_CURRENT_BINARY_DIR}
       --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
       -I ${CACHE_PROTO_PATH}
       --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
       ${CACHE_PROTO}
  DEPENDS ${CACHE_PROTO} protoc grpc_cpp_plugin
)

add_library(cache_proto_lib ${CACHE_PROTO_SRCS} ${CACHE_GRPC_SRCS})
target_include_directories(cache_proto_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(cache_proto_lib PUBLIC libprotobuf grpc++)

add_library(cache_grpc_lib src/protocol/grpc/cache_service.cpp)
target_include_directories(cache_grpc_lib PUBLIC ${PROJECT_SOURCE_DIR}/src
                                                 ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(cache_grpc_lib PUBLIC cache_protocol_lib cache_proto_lib)

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

add_executable(test_resp_parser tests/protocol/resp_parser_test.cpp)
target_link_libraries(test_resp_parser PRIVATE gtest_main cache_protocol_lib)
add_test(NAME RESPParser COMMAND test_resp_parser)

add_executable(test_resp_set_get tests/integration/resp_set_get_test.cpp)
target_link_libraries(test_resp_set_get PRIVATE gtest_main cache_protocol_lib)
add_test(NAME resp_set_get COMMAND test_resp_set_get)

add_executable(test_grpc_cache_service tests/protocol/grpc_cache_service_test.cpp)
target_link_libraries(test_grpc_cache_service PRIVATE gtest_main
                                                     cache_grpc_lib)
add_test(NAME GRPCCacheService COMMAND test_grpc_cache_service)

add_executable(test_hash_ring tests/cluster/hash_ring_test.cpp)
target_link_libraries(test_hash_ring PRIVATE gtest_main cache_cluster_lib)
add_test(NAME HashRing COMMAND test_hash_ring)

add_executable(test_wal_replication tests/replication/wal_replication_test.cpp)
target_link_libraries(test_wal_replication PRIVATE gtest_main
                                                   cache_replication_lib)
add_test(NAME WALReplication COMMAND test_wal_replication)

add_executable(test_ack_policy tests/replication/ack_mode_test.cpp)
target_link_libraries(test_ack_policy PRIVATE gtest_main cache_replication_lib)
add_test(NAME AckPolicy COMMAND test_ack_policy)

add_executable(test_request_coalescer tests/core/request_coalescer_test.cpp)
target_link_libraries(test_request_coalescer PRIVATE gtest_main cache_core_lib)
add_test(NAME RequestCoalescer COMMAND test_request_coalescer)

add_executable(test_failover_metadata tests/control/failover_metadata_test.cpp)
target_link_libraries(test_failover_metadata PRIVATE gtest_main cache_control_lib)
add_test(NAME FailoverMetadata COMMAND test_failover_metadata)

add_executable(test_prometheus_endpoint tests/metrics/prometheus_endpoint_test.cpp)
target_link_libraries(test_prometheus_endpoint PRIVATE gtest_main
                                                      cache_metrics_lib)
add_test(NAME Prometheus COMMAND test_prometheus_endpoint)
