# Expose test sources to parent scope for unified test executable
set(AKNET_CORE_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/core_tests.cpp
        PARENT_SCOPE
)

add_executable(aknet_core_tests
        core_tests.cpp
)

target_link_libraries(aknet_core_tests
        PRIVATE
        aknet_core
        Catch2::Catch2WithMain
)

target_compile_features(aknet_core_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(aknet_core_tests)