# Expose test sources to parent scope for unified test executable
set(AKNET_LOGGER_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/logger_tests.cpp
        PARENT_SCOPE
)

add_executable(aknet_logger_tests
        logger_tests.cpp
)

target_link_libraries(aknet_logger_tests
        PRIVATE
        aknet_logger
        Catch2::Catch2WithMain
)

target_compile_features(aknet_logger_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(aknet_logger_tests)