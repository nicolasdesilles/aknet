cmake_minimum_required(VERSION 3.21 FATAL_ERROR)


# Create the project
project(aknet LANGUAGES CXX VERSION 0.1.0)

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
# Create executable
# --------------------------------------------------------------------------------------------------------

set(CMAKE_BUILD_TYPE Debug)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  src/main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------------------------------------------------------------
# Add and link libraries
# --------------------------------------------------------------------------------------------------------

CPMAddPackage(
  NAME           saucer
  VERSION        7.0.2
  GIT_REPOSITORY "https://github.com/saucer/saucer"
)

target_link_libraries(${PROJECT_NAME} PRIVATE saucer::saucer)





