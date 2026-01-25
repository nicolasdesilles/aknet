# Expose test sources to parent scope for unified test executable
set(AKNET_JACK_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/jack_tests.cpp
        PARENT_SCOPE
)

add_executable(aknet_jack_tests
        jack_tests.cpp
)

target_link_libraries(aknet_jack_tests
        PRIVATE
        aknet_jack
        Catch2::Catch2WithMain
)

target_compile_features(aknet_jack_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(aknet_jack_tests)