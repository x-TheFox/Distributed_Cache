include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_CSHARP_EXT OFF CACHE BOOL "" FORCE)
set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "" FORCE)

FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG v1.67.0
  GIT_SHALLOW TRUE
  GIT_SUBMODULES_RECURSE ON
)

FetchContent_MakeAvailable(grpc)

if(APPLE)
  foreach(randen_target IN ITEMS absl_random_internal_randen_hwaes
                                 absl_random_internal_randen_hwaes_impl)
    if(TARGET ${randen_target})
      get_target_property(randen_opts ${randen_target} COMPILE_OPTIONS)
      if(randen_opts)
        list(REMOVE_ITEM randen_opts "-Xarch_x86_64" "-maes" "-msse4.1")
        set_property(TARGET ${randen_target} PROPERTY COMPILE_OPTIONS
                                                      "${randen_opts}")
      endif()
    endif()
  endforeach()
endif()
