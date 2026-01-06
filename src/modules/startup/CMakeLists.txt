# Library target
add_library(aknet_startup STATIC)

# Source files
target_sources(aknet_startup
        PRIVATE
        src/startup.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/startup.h
)

# Properties
target_include_directories(aknet_startup
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        src
)

target_compile_features(aknet_startup PRIVATE cxx_std_23)

# External dependencies
# N/A