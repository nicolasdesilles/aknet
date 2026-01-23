# Collect all test source files
file(GLOB STARTUP_TEST_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

# Expose to parent for unified executable
set(AKNET_STARTUP_TEST_SOURCES
    ${STARTUP_TEST_FILES}
    PARENT_SCOPE
)

# Per-module test executable
add_executable(aknet_startup_tests
    ${STARTUP_TEST_FILES}
)

target_link_libraries(aknet_startup_tests
    PRIVATE
    aknet_startup
    Catch2::Catch2WithMain
)

target_compile_features(aknet_startup_tests PRIVATE cxx_std_23)

# Make helpers available to test files
target_include_directories(aknet_startup_tests 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

include(CTest)
include(Catch)
catch_discover_tests(aknet_startup_tests)