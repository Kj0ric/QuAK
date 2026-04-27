cmake_minimum_required(VERSION 3.9)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are disabled. Use: cmake -S . -B build")
endif()

project(QuAK)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, defaulting to Release")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release)" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

OPTION(ENABLE_SCC_SEARCH_OPT "Enable SCC search optimization in FORKLIFT" ON)
# OPTION(ENABLE_CONTEXT_REDUNDANCY_OPT "Enable context redundancy optimization in FORKLIFT" OFF)

add_compile_definitions(WEIGHT_EQ_EPSILON=1e-5f)

if (ENABLE_SCC_SEARCH_OPT)
    add_compile_definitions(INCLUSION_SCC_SEARCH_ACTIVE)
endif()
# if (ENABLE_CONTEXT_REDUNDANCY_OPT)
#     add_compile_definitions(CONTEXT_REDUNDANCY_ACTIVE)
# endif()

OPTION(ENABLE_IPO "Enable interprocedural optimizations" ON)

if (ENABLE_IPO)
	cmake_policy(SET CMP0069 NEW)
	include(CheckIPOSupported)
	check_ipo_supported()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif(ENABLE_IPO)

add_subdirectory(src)

# =============================================================================
# MAIN CLI EXECUTABLE (default target)
# =============================================================================
# The main CLI supports both non-nested and nested automata
add_executable(quak-nested src/quak-nested-main.cpp)
target_include_directories(quak-nested PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(quak-nested PUBLIC quak quak-private)

# =============================================================================
# TARGET HELPERS
# =============================================================================
function(add_quak_test TEST_SOURCE)
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${TEST_NAME} PUBLIC quak quak-private)
    if(TEST_NAME STREQUAL "test_error_handling")
        add_dependencies(${TEST_NAME} quak-nested)
        target_compile_definitions(${TEST_NAME} PRIVATE QUAK_NESTED_PATH="$<TARGET_FILE:quak-nested>")
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_target_properties(${TEST_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set(QUAK_TEST_TARGETS ${QUAK_TEST_TARGETS} ${TEST_NAME} PARENT_SCOPE)
endfunction()

function(add_quak_experiment TARGET_NAME SOURCE)
    add_executable(${TARGET_NAME} ${SOURCE})
    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/tests
    )
    target_link_libraries(${TARGET_NAME} PUBLIC quak quak-private)
    set_target_properties(${TARGET_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set(QUAK_EXPERIMENT_TARGETS ${QUAK_EXPERIMENT_TARGETS} ${TARGET_NAME} PARENT_SCOPE)
endfunction()

function(add_quak_example EXAMPLE_SOURCE)
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${EXAMPLE_NAME} PUBLIC quak quak-private)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set(QUAK_EXAMPLE_TARGETS ${QUAK_EXAMPLE_TARGETS} ${EXAMPLE_NAME} PARENT_SCOPE)
endfunction()

# =============================================================================
# TESTS (built with: make tests)
# =============================================================================
enable_testing()

# Sanity tests for nested automata
set(SANITY_TEST_SOURCES
    src/tests/sanity_tests/test_flatten_regular.cpp
    src/tests/sanity_tests/test_flatten_avg_summinus.cpp
    src/tests/sanity_tests/test_flatten_sumplusminus_sup.cpp
    src/tests/sanity_tests/test_flatten_sumplusminus_inf.cpp
    src/tests/sanity_tests/test_flatten_minmax_sup.cpp
    src/tests/sanity_tests/test_flatten_minmax_inf.cpp
    src/tests/sanity_tests/test_emptiness_universality.cpp
    src/tests/sanity_tests/test_pseudo_determinization.cpp
    src/tests/sanity_tests/test_synchronization.cpp
    src/tests/sanity_tests/test_auxiliary_functions.cpp
)

foreach(TEST_SOURCE ${SANITY_TEST_SOURCES})
    add_quak_test(${TEST_SOURCE})
endforeach()

# Correctness tests for nested automata emptiness/universality
set(CORRECTNESS_TEST_SOURCES
    src/tests/correctness_tests/test_emptiness_correctness.cpp
    src/tests/correctness_tests/test_sum_sup_witness_edge_cases.cpp
    src/tests/correctness_tests/test_backend_regression_cases.cpp
    src/tests/correctness_tests/test_universality_correctness.cpp
    src/tests/correctness_tests/test_error_handling.cpp
)

foreach(TEST_SOURCE ${CORRECTNESS_TEST_SOURCES})
    add_quak_test(${TEST_SOURCE})
endforeach()

# Custom target to build all tests
add_custom_target(tests
    DEPENDS ${QUAK_TEST_TARGETS}
    COMMENT "Building all test executables"
)

# =============================================================================
# SMOKE TEST (built+run with: make smoke-test)
# Runs the representative smoke test + backward-compat test only (~1 s).
# =============================================================================
add_custom_target(smoke-test
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/smoke-test.sh
    DEPENDS quak-nested quak-experiment-single
    COMMENT "Running smoke test (quick mode)"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# =============================================================================
# EXPERIMENTS (built with: make experiments)
# =============================================================================
# Single automaton experiment runner (detailed analysis)
add_quak_experiment(quak-experiment-single src/quak-experiment-single.cpp)

# Experimental backend comparison tools
set(EXPERIMENT_TOOL_SOURCES
    src/tests/probes/sum_inf_fix_compare.cpp
    src/tests/probes/sum_sup_fix_compare.cpp
    src/tests/probes/sum_inf_backend_probe.cpp
    src/tests/probes/sum_inf_stability_stress.cpp
)

foreach(EXPERIMENT_SOURCE ${EXPERIMENT_TOOL_SOURCES})
    get_filename_component(EXPERIMENT_NAME ${EXPERIMENT_SOURCE} NAME_WE)
    add_quak_experiment(${EXPERIMENT_NAME} ${EXPERIMENT_SOURCE})
endforeach()

add_quak_experiment(minmax_sup_fix_compare src/tests/probes/minmax_sup_fix_compare.cpp)

add_quak_experiment(minmax_sup_fix_compare_cached src/tests/probes/minmax_sup_fix_compare.cpp)
target_compile_definitions(minmax_sup_fix_compare_cached PRIVATE MINMAX_SUP_USE_CACHED_MMTHR=1)

add_quak_experiment(minmax_sup_fix_compare_witness_cached src/tests/probes/minmax_sup_fix_compare.cpp)
target_compile_definitions(minmax_sup_fix_compare_witness_cached PRIVATE MINMAX_SUP_USE_WITNESS_CACHED=1)

add_quak_experiment(sum_sup_fix_compare_witness_cached src/tests/probes/sum_sup_fix_compare.cpp)
target_compile_definitions(sum_sup_fix_compare_witness_cached PRIVATE SUM_SUP_USE_WITNESS_CACHED=1)

add_quak_experiment(sum_sup_fix_compare_full src/tests/probes/sum_sup_fix_compare.cpp)
target_compile_definitions(sum_sup_fix_compare_full PRIVATE SUM_SUP_FULL_HARNESS=1 SUM_SUP_RUN_REGULAR_ORACLE=0)

add_quak_experiment(sum_sup_fix_compare_witness_cached_full src/tests/probes/sum_sup_fix_compare.cpp)
target_compile_definitions(sum_sup_fix_compare_witness_cached_full PRIVATE SUM_SUP_USE_WITNESS_CACHED=1 SUM_SUP_FULL_HARNESS=1 SUM_SUP_RUN_REGULAR_ORACLE=0)

add_quak_experiment(minmax_sup_resource_probe src/tests/probes/minmax_sup_resource_probe.cpp)

# Custom target to build all experiments
add_custom_target(experiments
    DEPENDS ${QUAK_EXPERIMENT_TARGETS}
    COMMENT "Building experiment executables"
)

# =============================================================================
# EXAMPLES (built with: make examples)
# =============================================================================
set(EXAMPLE_SOURCES
    examples/example1_basic.cpp
    examples/example2_value_functions.cpp
    examples/example3_response_time.cpp
)

foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    add_quak_example(${EXAMPLE_SOURCE})
endforeach()

# Custom target to build all examples
add_custom_target(examples
    DEPENDS ${QUAK_EXAMPLE_TARGETS}
    COMMENT "Building example programs"
)

# =============================================================================
# BENCHMARK (built with: make benchmark)
# Uncomment to enable benchmark target
# =============================================================================
# add_executable(benchmark_oblbag src/tests/benchmarks/benchmark_oblbag.cpp)
# target_include_directories(benchmark_oblbag PRIVATE ${CMAKE_SOURCE_DIR}/src)
# target_link_libraries(benchmark_oblbag PUBLIC quak quak-private)
# set_target_properties(benchmark_oblbag PROPERTIES EXCLUDE_FROM_ALL TRUE)
#
# add_custom_target(benchmark
#     DEPENDS benchmark_oblbag
#     COMMENT "Building benchmark executable"
# )
