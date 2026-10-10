cmake_minimum_required(VERSION 3.20)
project(n64_plugin LANGUAGES CXX)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(VELOCE_CORES_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/cores)
endif()
add_library(n64_plugin SHARED src/plugin.cpp src/machine.cpp src/vr4300.cpp)
target_compile_features(n64_plugin PRIVATE cxx_std_17)
target_include_directories(n64_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set_target_properties(n64_plugin PROPERTIES PREFIX "" OUTPUT_NAME "n64"
    LIBRARY_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY})
foreach(config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${config} upper)
    set_target_properties(n64_plugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${upper} ${VELOCE_CORES_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_${upper} ${VELOCE_CORES_OUTPUT_DIRECTORY})
endforeach()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(n64_plugin PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(n64_plugin PRIVATE /W4)
endif()
