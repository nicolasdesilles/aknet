# This is for external dependencies for our project

# --------------------------------------------------------------------------------------------------------
# CPM.make
# --------------------------------------------------------------------------------------------------------

# Download CPM.cmake
file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.8/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=78ba32abdf798bc616bab7c73aac32a17bbd7b06ad9e26a6add69de8f3ae4791
)

# Include CPM.cmake
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# --------------------------------------------------------------------------------------------------------
# Saucer
# --------------------------------------------------------------------------------------------------------

CPMAddPackage(
        NAME           saucer
        VERSION        7.0.2
        GIT_REPOSITORY "https://github.com/saucer/saucer"
)

# --------------------------------------------------------------------------------------------------------
# spdlog
# --------------------------------------------------------------------------------------------------------

CPMAddPackage(
        NAME           spdlog
        VERSION        1.16.0
        GIT_REPOSITORY "https://github.com/gabime/spdlog"
)

# --------------------------------------------------------------------------------------------------------
# Catch2
# --------------------------------------------------------------------------------------------------------

# Using CPM_DONT_UPDATE_MODULE_PATH option and manually adding the module path because CMake would not find it otherwise.
# Maybe sketchy, idk
CPMAddPackage(
        NAME           Catch2
        VERSION        3.11.0
        GIT_REPOSITORY "https://github.com/catchorg/Catch2"
        CPM_DONT_UPDATE_MODULE_PATH
)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/_deps/catch2-src/extras)

# --------------------------------------------------------------------------------------------------------
# JSON
# --------------------------------------------------------------------------------------------------------

CPMAddPackage("gh:nlohmann/json@3.12.0")

# --------------------------------------------------------------------------------------------------------
# JACK (system dependency)
# --------------------------------------------------------------------------------------------------------

option(AKNET_ENABLE_JACK "Enable building with JACK (libjack)" ON)

if(AKNET_ENABLE_JACK)
    # ---- Homebrew hints (macOS) ----
    if(APPLE)
        # Prefer asking brew for its prefix, but fall back to common defaults.
        find_program(HOMEBREW_EXECUTABLE brew)
        if(HOMEBREW_EXECUTABLE)
            execute_process(
                    COMMAND "${HOMEBREW_EXECUTABLE}" --prefix
                    OUTPUT_VARIABLE HOMEBREW_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
            )
        endif()

        if(NOT HOMEBREW_PREFIX OR HOMEBREW_PREFIX STREQUAL "")
            if(EXISTS "/opt/homebrew")
                set(HOMEBREW_PREFIX "/opt/homebrew")
            elseif(EXISTS "/usr/local")
                set(HOMEBREW_PREFIX "/usr/local")
            endif()
        endif()

        if(HOMEBREW_PREFIX AND NOT HOMEBREW_PREFIX STREQUAL "")
            # Help CMake find headers/libs installed by brew.
            list(PREPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")

            # Help pkg-config find jack.pc (common locations).
            set(_brew_pkgconfig_paths
                    "${HOMEBREW_PREFIX}/lib/pkgconfig"
                    "${HOMEBREW_PREFIX}/share/pkgconfig"
            )
            foreach(_p IN LISTS _brew_pkgconfig_paths)
                if(EXISTS "${_p}")
                    if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
                        set(ENV{PKG_CONFIG_PATH} "${_p}:$ENV{PKG_CONFIG_PATH}")
                    else()
                        set(ENV{PKG_CONFIG_PATH} "${_p}")
                    endif()
                endif()
            endforeach()
        endif()
    endif()

    # ---- Prefer pkg-config if available ----
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        # Creates an imported target PkgConfig::JACK when found.
        pkg_check_modules(JACK QUIET IMPORTED_TARGET jack)
    endif()

    if(TARGET PkgConfig::JACK)
        add_library(jack::jack ALIAS PkgConfig::JACK)
        message(STATUS "Found libjack via pkg-config (target: jack::jack)")
    else()
        # ---- Fallback: manual search ----
        find_path(JACK_INCLUDE_DIR
                NAMES jack/jack.h
        )

        find_library(JACK_LIBRARY
                NAMES jack
        )

        if(NOT JACK_INCLUDE_DIR OR NOT JACK_LIBRARY)
            message(FATAL_ERROR
                    "AKNET_ENABLE_JACK is ON but libjack was not found.\n"
                    "On macOS (Homebrew), try:\n"
                    "  brew install jack\n"
                    "Then reconfigure. You may also need PKG_CONFIG_PATH set to your brew prefix.\n"
            )
        endif()

        add_library(jack::jack UNKNOWN IMPORTED)
        set_target_properties(jack::jack PROPERTIES
                IMPORTED_LOCATION "${JACK_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${JACK_INCLUDE_DIR}"
        )

        message(STATUS "Found libjack via find_library/find_path (target: jack::jack)")
    endif()
endif()