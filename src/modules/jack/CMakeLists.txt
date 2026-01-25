# Library target
add_library(aknet_jack STATIC)

# Source files
target_sources(aknet_jack
        PRIVATE
        src/jack.cpp
        src/jack_client.cpp
        src/jack_server_manager.cpp
        src/posix_process_runner.cpp
        src/libjack_client_api.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/jack.h
        include/jack_client.h
        include/jack_interfaces.h
        include/jack_server_manager.h
        include/posix_process_runner.h
        include/libjack_client_api.h
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
target_link_libraries(aknet_jack PUBLIC
        aknet_logger
        aknet_settings
        jack::jack
)