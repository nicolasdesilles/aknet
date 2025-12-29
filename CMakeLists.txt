cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

# Create the project
project(aknet LANGUAGES CXX VERSION 0.1.0)

# For reference in C++ code via template header file in core lib
set (PROJECT_VERSION ${CMAKE_PROJECT_VERSION})
set (PROJECT_NAME ${CMAKE_PROJECT_NAME})
string(TIMESTAMP TODAY "%d/%m/%Y")
set (PROJECT_BUILD_DATE ${TODAY})
set (PROJECT_AUTHOR "Nicolas Désilles @laplanque")

# --------------------------------------------------------------------------------------------------------
# C++ Standard
# --------------------------------------------------------------------------------------------------------

# C++23 Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------------------------------------
# Output Directories and CMake Helpers
# --------------------------------------------------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include CMake helpers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerOptions)
include(Dependencies)

# --------------------------------------------------------------------------------------------------------
# Setup Testing
# --------------------------------------------------------------------------------------------------------

# Enable testing support
include(CTest)
enable_testing()

# Option to build tests
option(AKNET_BUILD_TESTS "Build unit tests" ON)

# --------------------------------------------------------------------------------------------------------
# Subdirectories
# --------------------------------------------------------------------------------------------------------

# Core library
add_subdirectory(src/core)

# Modules

# Utils
add_subdirectory(src/utils/logger)

# We make the utilities available to all modules

# --------------------------------------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------------------------------------
# --------------------------------------------------------------------------------------------------------

if(AKNET_BUILD_TESTS)
    # Module tests
    add_subdirectory(src/core/tests)
    add_subdirectory(src/utils/logger/tests)

    # Integration tests
    add_subdirectory(tests)

    # Unified test executable with ALL tests
    add_executable(aknet_all_tests
            ${AKNET_LOGGER_TEST_SOURCES}
            ${AKNET_CORE_TEST_SOURCES}
            ${AKNET_INTEGRATION_TEST_SOURCES}
    )

    target_link_libraries(aknet_all_tests
            PRIVATE
            aknet_core
            aknet_logger
            Catch2::Catch2WithMain
    )

    target_compile_features(aknet_all_tests PRIVATE cxx_std_23)
endif()

# --------------------------------------------------------------------------------------------------------
# Create executable
# --------------------------------------------------------------------------------------------------------

if(APPLE)

    set(CMAKE_BUILD_TYPE Debug)

    add_executable(${PROJECT_NAME}
            src/main.cpp
    )

    target_link_libraries(aknet
            PRIVATE
            aknet_core
            aknet_logger
            saucer::saucer
            saucer::embedded
    )

    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
else ()
    message(FATAL_ERROR "Only building on/for macOS is supported for now. Exiting.")
endif()

# --------------------------------------------------------------------------------------------------------
# Saucer Embed
# --------------------------------------------------------------------------------------------------------

saucer_embed("src/ui/dist")

# --------------------------------------------------------------------------------------------------------
# Code Coverage (only for aknet_all_tests)
# --------------------------------------------------------------------------------------------------------

if(ENABLE_COVERAGE)
    # Find llvm-cov and llvm-profdata
    find_program(LLVM_COV_PATH llvm-cov)
    find_program(LLVM_PROFDATA_PATH llvm-profdata)

    if(LLVM_COV_PATH AND LLVM_PROFDATA_PATH)
        message(STATUS "Found llvm-cov: ${LLVM_COV_PATH}")
        message(STATUS "Found llvm-profdata: ${LLVM_PROFDATA_PATH}")

        set(COVERAGE_IGNORE_REGEX "build/.*|build_coverage/.*|_deps/.*|.*tests/.*")

        # Add custom target for generating coverage report
        add_custom_target(coverage
                COMMAND ${CMAKE_COMMAND} -E echo "Running tests..."
                COMMAND LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage.profraw $<TARGET_FILE:aknet_all_tests>

                COMMAND ${CMAKE_COMMAND} -E echo "Merging coverage data..."
                COMMAND ${LLVM_PROFDATA_PATH} merge -sparse ${CMAKE_BINARY_DIR}/coverage.profraw -o ${CMAKE_BINARY_DIR}/coverage.profdata

                COMMAND ${CMAKE_COMMAND} -E echo "Generating HTML coverage report..."
                COMMAND ${LLVM_COV_PATH} show $<TARGET_FILE:aknet_all_tests>
                -instr-profile ${CMAKE_BINARY_DIR}/coverage.profdata
                -format html
                -output-dir ${CMAKE_BINARY_DIR}/coverage_report
                -ignore-filename-regex="${COVERAGE_IGNORE_REGEX}"
                ${CMAKE_SOURCE_DIR}/src

                COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage summary..."
                COMMAND ${LLVM_COV_PATH} report $<TARGET_FILE:aknet_all_tests>
                -instr-profile ${CMAKE_BINARY_DIR}/coverage.profdata
                -ignore-filename-regex="${COVERAGE_IGNORE_REGEX}"

                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                DEPENDS aknet_all_tests
                COMMENT "Generating code coverage report..."
        )

    else()
        message(WARNING "llvm-cov or llvm-profdata not found. Coverage target will not be available.")
    endif()
endif()








