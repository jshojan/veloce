cmake_minimum_required(VERSION 3.16)
project(gb_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect if we're being built standalone or as part of Veloce
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GB_PLUGIN_STANDALONE TRUE)
    message(STATUS "Building GB plugin standalone")

    # Set default output directory for standalone builds
    if(NOT DEFINED VELOCE_CORES_OUTPUT_DIRECTORY)
        set(VELOCE_CORES_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/cores)
    endif()
else()
    set(GB_PLUGIN_STANDALONE FALSE)
endif()

# GB Plugin as shared library
# Supports Game Boy (DMG) and Game Boy Color (CGB)
add_library(gb_plugin SHARED
    src/plugin.cpp
    src/lr35902.cpp
    src/ppu.cpp
    src/apu.cpp
    src/bus.cpp
    src/cartridge.cpp
    src/mbc/mbc.cpp
    src/mbc/mbc1.cpp
    src/mbc/mbc3.cpp
    src/mbc/mbc5.cpp
)

# Ensure the is_debug_mode() function is available
target_compile_definitions(gb_plugin PRIVATE)

target_include_directories(gb_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Include the main Veloce headers
if(GB_PLUGIN_STANDALONE)
    # When building standalone, the headers are two directories up
    set(VELOCE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    if(EXISTS ${VELOCE_INCLUDE_DIR})
        target_include_directories(gb_plugin PRIVATE ${VELOCE_INCLUDE_DIR})
    else()
        message(WARNING "Veloce include directory not found at ${VELOCE_INCLUDE_DIR}")
    endif()
else()
    # When building as part of Veloce, use CMAKE_SOURCE_DIR
    target_include_directories(gb_plugin PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

# Set output name without 'lib' prefix on all platforms
# Emulator cores go to the cores/ directory
set_target_properties(gb_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gb"
    LIBRARY_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY}
)

# Handle multi-config generators (Visual Studio, Xcode)
foreach(CONFIG_TYPE IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(gb_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_CORES_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_CORES_OUTPUT_DIRECTORY}
    )
endforeach()

# Platform-specific settings
if(WIN32)
    set_target_properties(gb_plugin PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(gb_plugin PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(gb_plugin PROPERTIES SUFFIX ".so")
endif()

# Compiler warnings for better code quality
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gb_plugin PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(gb_plugin PRIVATE /W4)
endif()

# Note: Test ROMs should be run through veloce directly with DEBUG=1 environment variable
# Example: DEBUG=1 timeout 30 ./build/bin/veloce cores/gb/tests/gb-test-roms/cpu_instrs/cpu_instrs.gb
# The emulator will detect test ROM results and report PASS/FAIL via debug output
