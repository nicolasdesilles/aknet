# Library target
add_library(aknet_jack STATIC)

if(APPLE)
    # Source files
    target_sources(aknet_jack
            PRIVATE
            src/jack.cpp
            src/jack_module.cpp
            src/jack_client.cpp
            src/jack_server_manager.cpp
            src/jack_audio_processor.cpp
            src/posix_process_runner.cpp
            src/libjack_client_api.cpp
            src/audio_device_manager.cpp
            src/coreaudio_device_manager.cpp
            src/startup_steps/check_jack_installation_step.cpp
            src/startup_steps/ensure_jack_server_step.cpp
            src/startup_steps/register_jack_client_step.cpp
            PUBLIC FILE_SET HEADERS
            BASE_DIRS include
            FILES
            include/jack.h
            include/jack_module.h
            include/jack_client.h
            include/jack_interfaces.h
            include/jack_server_manager.h
            include/jack_audio_processor.h
            include/posix_process_runner.h
            include/libjack_client_api.h
            include/audio_device_manager.h
            include/coreaudio_device_manager.h
            include/startup_steps/check_jack_installation_step.h
            include/startup_steps/ensure_jack_server_step.h
            include/startup_steps/register_jack_client_step.h
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
            aknet_startup
            jack::jack
            "-framework CoreAudio"
            "-framework CoreFoundation"
    )
endif()