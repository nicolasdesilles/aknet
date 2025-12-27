if(APPLE)
    # Use libc++ for C++23 modules support
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)

    # Module support (C++23)
    add_compile_options(-fmodules -fbuiltin-module-map)

    # Stack size for real-time audio
    add_link_options(-Wl,-stack_size,0x1000000)
endif()

# Optimization settings
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native)
    # Optional: Link-time optimization (LTO)
    # add_compile_options(-flto)
    # add_link_options(-flto)
else()
    # Debug build: minimal optimization, debug symbols
    add_compile_options(-g -O0)
endif()
