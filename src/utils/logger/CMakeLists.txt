# Library target
add_library(aknet_logger STATIC)

# Source files
target_sources(aknet_logger
        PRIVATE
        src/logger.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/logger.h
)

# Properties
target_include_directories(aknet_logger
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        src
)

target_compile_features(aknet_logger PRIVATE cxx_std_23)

# External dependencies
target_link_libraries(aknet_logger PUBLIC spdlog::spdlog)

