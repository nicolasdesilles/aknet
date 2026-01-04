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