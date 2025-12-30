# Expose test sources to parent scope for unified test executable
set(AKNET_INTEGRATION_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests.cpp
        PARENT_SCOPE
)

add_executable(aknet_integration_tests
        integration_tests.cpp
)

target_link_libraries(aknet_integration_tests
        PRIVATE
        aknet_core
        aknet_logger
        Catch2::Catch2WithMain
)

target_compile_features(aknet_integration_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(aknet_integration_tests)