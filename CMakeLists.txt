cmake_minimum_required(VERSION 3.9)

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
# The main CLI supports both regular and nested automata
add_executable(quak-nested src/quak-nested-main.cpp)
target_include_directories(quak-nested PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(quak-nested PUBLIC quak quak-private)

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
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${TEST_NAME} PUBLIC quak quak-private)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_target_properties(${TEST_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endforeach()

# Combined sanity test (all sanity tests in one executable)
add_executable(test_sanity_all src/tests/sanity_tests/test_sanity_all.cpp)
target_include_directories(test_sanity_all PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sanity_all PUBLIC quak quak-private)
add_test(NAME test_sanity_all COMMAND test_sanity_all WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_target_properties(test_sanity_all PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Correctness tests for nested automata emptiness/universality
set(CORRECTNESS_TEST_SOURCES
    src/tests/correctness_tests/test_emptiness_correctness.cpp
    src/tests/correctness_tests/test_universality_correctness.cpp
    src/tests/correctness_tests/test_non_nested_backward_compat.cpp
)

foreach(TEST_SOURCE ${CORRECTNESS_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${TEST_NAME} PUBLIC quak quak-private)
    if(TEST_NAME STREQUAL "test_non_nested_backward_compat")
        add_dependencies(${TEST_NAME} quak-nested)
        target_compile_definitions(${TEST_NAME} PRIVATE QUAK_NESTED_PATH="$<TARGET_FILE:quak-nested>")
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_target_properties(${TEST_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endforeach()

# Custom target to build all tests
add_custom_target(tests
    DEPENDS
        quak-nested
        test_sanity_all
        test_flatten_regular
        test_flatten_avg_summinus
        test_flatten_sumplusminus_sup
        test_flatten_sumplusminus_inf
        test_flatten_minmax_sup
        test_flatten_minmax_inf
        test_emptiness_universality
        test_pseudo_determinization
        test_synchronization
        test_auxiliary_functions
        test_emptiness_correctness
        test_universality_correctness
        test_non_nested_backward_compat
    COMMENT "Building all test executables"
)

# =============================================================================
# EXPERIMENTS (built with: make experiments)
# =============================================================================
# Single automaton experiment runner (detailed analysis)
add_executable(quak-experiment-single src/quak-experiment-single.cpp)
target_include_directories(quak-experiment-single PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(quak-experiment-single PUBLIC quak quak-private)
set_target_properties(quak-experiment-single PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Custom target to build all experiments
add_custom_target(experiments
    DEPENDS quak-experiment-single
    COMMENT "Building experiment executables"
)

# =============================================================================
# EXAMPLES (built with: make examples)
# =============================================================================
set(EXAMPLE_SOURCES
    examples/nested/example1_basic.cpp
    examples/nested/example2_value_functions.cpp
    examples/nested/example3_response_time.cpp
)

foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_include_directories(${EXAMPLE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${EXAMPLE_NAME} PUBLIC quak quak-private)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endforeach()

# Custom target to build all examples
add_custom_target(examples
    DEPENDS example1_basic example2_value_functions example3_response_time
    COMMENT "Building example programs"
)

# =============================================================================
# BENCHMARK (built with: make benchmark)
# Uncomment to enable benchmark target
# =============================================================================
# add_executable(benchmark_oblbag src/tests/benchmark_oblbag.cpp)
# target_include_directories(benchmark_oblbag PRIVATE ${CMAKE_SOURCE_DIR}/src)
# target_link_libraries(benchmark_oblbag PUBLIC quak quak-private)
# set_target_properties(benchmark_oblbag PROPERTIES EXCLUDE_FROM_ALL TRUE)
#
# add_custom_target(benchmark
#     DEPENDS benchmark_oblbag
#     COMMENT "Building benchmark executable"
# )
