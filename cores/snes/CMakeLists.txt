cmake_minimum_required(VERSION 3.16)
project(snes_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SNES Plugin as shared library
add_library(snes_plugin SHARED
    src/snes_plugin.cpp
    src/cpu.cpp
    src/ppu.cpp
    src/apu.cpp
    src/dsp.cpp
    src/spc700.cpp
    src/bus.cpp
    src/dma.cpp
    src/cartridge.cpp
)

target_include_directories(snes_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Set output name without 'lib' prefix on all platforms
# Emulator cores go to the cores/ directory
set_target_properties(snes_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "snes"
    LIBRARY_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY}
)

# Handle multi-config generators (Visual Studio, Xcode)
foreach(CONFIG_TYPE IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(snes_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_CORES_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_CORES_OUTPUT_DIRECTORY}
    )
endforeach()

# Platform-specific settings
if(WIN32)
    set_target_properties(snes_plugin PROPERTIES SUFFIX ".dll")
elseif(APPLE)
    set_target_properties(snes_plugin PROPERTIES SUFFIX ".dylib")
else()
    set_target_properties(snes_plugin PROPERTIES SUFFIX ".so")
endif()
