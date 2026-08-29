cmake_minimum_required(VERSION 3.24)
project(quiet_trace_host_tests LANGUAGES CXX)

enable_testing()

add_executable(
  aggregate_contract_test
  aggregate_contract_test.cpp
  ../components/domain/aggregate_contract.cpp
)
target_compile_features(aggregate_contract_test PRIVATE cxx_std_20)
target_compile_options(
  aggregate_contract_test
  PRIVATE -Wall -Wextra -Wpedantic -Werror
)
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
target_compile_options(
  synthetic_source_test
  PRIVATE -Wall -Wextra -Wpedantic -Werror
)
target_include_directories(
  synthetic_source_test
  PRIVATE ../components/audio_source/include support
)

add_test(NAME synthetic_source COMMAND synthetic_source_test)
