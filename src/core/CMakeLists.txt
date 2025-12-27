# Core application library

add_library(aknet_core STATIC)

target_sources(aknet_core
        PRIVATE
        src/core.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/core.h
        include/config.h
)

target_include_directories(aknet_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(aknet_core PRIVATE cxx_std_23)
set_target_properties(aknet_core PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
