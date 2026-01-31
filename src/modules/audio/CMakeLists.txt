# Library target
add_library(aknet_audio STATIC)

# Source files
target_sources(aknet_audio
        PRIVATE
        src/audio.cpp
        src/audio_module.cpp
        src/audio_buffer.cpp
        PUBLIC FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/audio.h
        include/audio_module.h
        include/audio_block_view.h
        include/audio_buffer.h
)

# Properties
target_include_directories(aknet_audio
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        src
)

target_compile_features(aknet_audio PRIVATE cxx_std_23)

# External dependencies
target_link_libraries(aknet_audio PUBLIC
        aknet_logger
        aknet_settings
)