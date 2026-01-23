# Expose test sources to parent scope for unified test executable
set(AKNET_BRIDGE_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/bridge_tests.cpp
        PARENT_SCOPE
)

add_executable(aknet_bridge_tests
        bridge_tests.cpp
)

target_link_libraries(aknet_bridge_tests
        PRIVATE
        aknet_bridge
        Catch2::Catch2WithMain
)

target_compile_features(aknet_bridge_tests PRIVATE cxx_std_23)

include(CTest)
include(Catch)
catch_discover_tests(aknet_bridge_tests)