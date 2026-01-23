# Library target
add_library(aknet_bridge STATIC)

# Source files
target_sources(aknet_bridge
        PRIVATE
        src/bridge.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/bridge.h
)


# Properties
target_include_directories(aknet_bridge
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        src
)

target_compile_features(aknet_bridge PRIVATE cxx_std_23)

# External dependencies
target_link_libraries(aknet_bridge PUBLIC
        aknet_logger
        aknet_settings
        aknet_startup
        nlohmann_json::nlohmann_json
        saucer::saucer )