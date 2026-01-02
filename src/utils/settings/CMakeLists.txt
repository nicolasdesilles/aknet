# Library target
add_library(aknet_settings STATIC)

# Source files
target_sources(aknet_settings
        PRIVATE
        src/settings.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/settings.h
)

# Properties
target_include_directories(aknet_settings
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        src
)

target_compile_features(aknet_settings PRIVATE cxx_std_23)

# External dependencies
target_link_libraries(aknet_settings PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(aknet_settings PRIVATE aknet_logger)

