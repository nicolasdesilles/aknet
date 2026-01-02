# Expose test sources to parent scope for unified test executable
set(AKNET_SETTINGS_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/settings_tests.cpp
        PARENT_SCOPE
)

add_executable(aknet_settings_tests
        settings_tests.cpp
)

target_link_libraries(aknet_settings_tests
        PRIVATE
        aknet_logger
        aknet_settings
        Catch2::Catch2WithMain
)

target_compile_features(aknet_settings_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(aknet_settings_tests)