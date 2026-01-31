# Collect all test source files
file(GLOB AUDIO_TEST_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

# Expose to parent for unified executable
set(AKNET_AUDIO_TEST_SOURCES
        ${AUDIO_TEST_FILES}
        PARENT_SCOPE
)

# Per-module test executable
add_executable(aknet_audio_tests
        ${AUDIO_TEST_FILES}
)

target_link_libraries(aknet_audio_tests
        PRIVATE
        aknet_audio
        Catch2::Catch2WithMain
)

target_compile_features(aknet_audio_tests PRIVATE cxx_std_23)

# Make helpers available to test files
target_include_directories(aknet_audio_tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/helpers
)

include(CTest)
include(Catch)
catch_discover_tests(aknet_audio_tests)