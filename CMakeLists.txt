cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

# Create the project
project(aknet LANGUAGES CXX VERSION 0.1.0)

# --------------------------------------------------------------------------------------------------------
# C++ Standard
# --------------------------------------------------------------------------------------------------------

# C++23 Standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------------------------------------------
# Output Directories and CMake Helpers
# --------------------------------------------------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include CMake helpers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerOptions)
include(Dependencies)

# --------------------------------------------------------------------------------------------------------
# Subdirectories
# --------------------------------------------------------------------------------------------------------

# Core library
add_subdirectory(src/core)

# Modules
add_subdirectory(src/modules/logger)

# --------------------------------------------------------------------------------------------------------
# Create executable
# --------------------------------------------------------------------------------------------------------

if(APPLE)

    set(CMAKE_BUILD_TYPE Debug)

    add_executable(${PROJECT_NAME}
            src/main.cpp
    )

    target_link_libraries(aknet
            PRIVATE
                aknet_core
                # aknet_logger
                saucer::saucer
                saucer::embedded
    )

    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)
else ()
    message(FATAL_ERROR "Only building on/for macOS is supported for now. Exiting.")
endif()

# --------------------------------------------------------------------------------------------------------
# Saucer Embed
# --------------------------------------------------------------------------------------------------------

saucer_embed("src/ui/dist")








