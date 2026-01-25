# Library target
add_library(aknet_jack STATIC)

# Source files
target_sources(aknet_jack
        PRIVATE
        src/jack.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/jack.h
)


# Properties
target_include_directories(aknet_jack
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        src
)

target_compile_features(aknet_jack PRIVATE cxx_std_23)

# External dependencies
target_link_libraries(aknet_bridge PUBLIC
        aknet_logger
        aknet_settings
        jack::jack
)