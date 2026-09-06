cmake_minimum_required(VERSION 3.24)
project(quiet_trace_host_tests LANGUAGES CXX)

enable_testing()

set(
  QUIET_TRACE_DOMAIN_SRCS
  ../components/domain/aggregate_contract.cpp
  ../components/domain/acquisition_pipeline.cpp
  ../components/domain/record_ring.cpp
  ../components/domain/control_plane.cpp
)

set(QUIET_TRACE_WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror)

add_executable(
  aggregate_contract_test
  aggregate_contract_test.cpp
  ${QUIET_TRACE_DOMAIN_SRCS}
)
target_compile_features(aggregate_contract_test PRIVATE cxx_std_20)
target_compile_options(aggregate_contract_test PRIVATE ${QUIET_TRACE_WARNING_FLAGS})
target_include_directories(
  aggregate_contract_test
  PRIVATE ../components/domain/include
)
add_test(NAME aggregate_contract COMMAND aggregate_contract_test)

add_executable(
  synthetic_source_test
  synthetic_source_test.cpp
)
target_compile_features(synthetic_source_test PRIVATE cxx_std_20)
target_compile_options(synthetic_source_test PRIVATE ${QUIET_TRACE_WARNING_FLAGS})
target_include_directories(
  synthetic_source_test
  PRIVATE ../components/audio_source/include support
)
add_test(NAME synthetic_source COMMAND synthetic_source_test)

add_executable(
  acquisition_pipeline_test
  acquisition_pipeline_test.cpp
  ${QUIET_TRACE_DOMAIN_SRCS}
)
target_compile_features(acquisition_pipeline_test PRIVATE cxx_std_20)
target_compile_options(acquisition_pipeline_test PRIVATE ${QUIET_TRACE_WARNING_FLAGS})
target_include_directories(
  acquisition_pipeline_test
  PRIVATE ../components/domain/include
)
add_test(NAME acquisition_pipeline COMMAND acquisition_pipeline_test)

add_executable(
  record_ring_test
  record_ring_test.cpp
  ${QUIET_TRACE_DOMAIN_SRCS}
)
target_compile_features(record_ring_test PRIVATE cxx_std_20)
target_compile_options(record_ring_test PRIVATE ${QUIET_TRACE_WARNING_FLAGS})
target_include_directories(
  record_ring_test
  PRIVATE ../components/domain/include
)
add_test(NAME record_ring COMMAND record_ring_test)

add_executable(
  control_plane_test
  control_plane_test.cpp
  ${QUIET_TRACE_DOMAIN_SRCS}
)
target_compile_features(control_plane_test PRIVATE cxx_std_20)
target_compile_options(control_plane_test PRIVATE ${QUIET_TRACE_WARNING_FLAGS})
target_include_directories(
  control_plane_test
  PRIVATE ../components/domain/include
)
add_test(NAME control_plane COMMAND control_plane_test)
