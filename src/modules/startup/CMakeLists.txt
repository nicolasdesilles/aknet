# Library target
add_library(aknet_startup STATIC)

# Source files
target_sources(aknet_startup
        PRIVATE
        src/startup.cpp
        src/startup_engine.cpp
        src/startup_manager.cpp
        src/startup_json.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/startup.h
        include/clock.h
        include/startup_step.h
        include/startup_engine.h
        include/startup_manager.h
        include/startup_json.h
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
target_link_libraries(aknet_startup PUBLIC
        aknet_logger
        aknet_settings
        nlohmann_json::nlohmann_json
        cr::ereignis
        saucer::fill  )