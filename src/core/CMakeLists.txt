# Core application library

configure_file (version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h @ONLY)
add_library(aknet_core STATIC)

target_sources(aknet_core
        PRIVATE
        src/core.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/core.h
        include/version.h
)

target_include_directories(aknet_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/startup_steps
)

# External dependencies
target_link_libraries(aknet_core PUBLIC
        aknet_logger
        aknet_settings
        aknet_startup
        aknet_bridge
        aknet_jack
        saucer::saucer
)



target_compile_features(aknet_core PRIVATE cxx_std_23)
set_target_properties(aknet_core PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
