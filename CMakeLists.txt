cmake_minimum_required(VERSION 3.16)
project(Veloce VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories (handle multi-config generators like Visual Studio)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Plugin output directories:
# - cores/   : Emulator core plugins (NES, GBA, SNES, etc.)
# - plugins/ : Other plugins (audio, input, TAS, speedrun tools, game plugins)
set(VELOCE_CORES_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/cores)
set(VELOCE_PLUGINS_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins)

# For multi-config generators (Visual Studio, Xcode)
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    add_definitions(-DPLATFORM_WINDOWS)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    add_definitions(-DPLATFORM_MACOS)
elseif(UNIX)
    set(PLATFORM_LINUX TRUE)
    add_definitions(-DPLATFORM_LINUX)
endif()

# Find required packages
find_package(OpenGL REQUIRED)

# SDL2 - try to find system installation first
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    message(STATUS "SDL2 not found, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-2.28.5
        GIT_SHALLOW TRUE
    )
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
    set(SDL2_LIBRARIES SDL2-static SDL2main)
    set(SDL2_INCLUDE_DIRS ${sdl2_SOURCE_DIR}/include)
else()
    set(SDL2_LIBRARIES SDL2::SDL2)
endif()

# Fetch Dear ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# ImGui library
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(imgui PUBLIC ${SDL2_LIBRARIES} OpenGL::GL)

# Main executable sources
set(CORE_SOURCES
    src/core/main.cpp
    src/core/application.cpp
    src/core/window_manager.cpp
    src/core/renderer.cpp
    src/core/input_manager.cpp
    src/core/audio_manager.cpp
    src/core/plugin_manager.cpp
    src/core/plugin_registry.cpp
    src/core/plugin_config.cpp
    src/core/paths_config.cpp
    src/core/speedrun_manager.cpp
    src/core/savestate_manager.cpp
    src/core/netplay_manager.cpp
    src/core/netplay_input_manager.cpp
)

set(GUI_SOURCES
    src/gui/gui_manager.cpp
    src/gui/main_menu.cpp
    src/gui/game_view.cpp
    src/gui/speedrun_panel.cpp
    src/gui/debug_panel.cpp
    src/gui/input_config_panel.cpp
    src/gui/plugin_config_panel.cpp
    src/gui/paths_config_panel.cpp
    src/gui/notification_manager.cpp
    src/gui/netplay_panel.cpp
)

# Main executable
add_executable(veloce
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)

target_include_directories(veloce PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(veloce PRIVATE
    ${SDL2_LIBRARIES}
    OpenGL::GL
    imgui
    nlohmann_json::nlohmann_json
)

# Platform-specific libraries
if(PLATFORM_LINUX)
    target_link_libraries(veloce PRIVATE dl)
elseif(PLATFORM_MACOS)
    target_link_libraries(veloce PRIVATE dl)
elseif(PLATFORM_WINDOWS)
    # Link SDL2main for Windows entry point
    if(TARGET SDL2main)
        target_link_libraries(veloce PRIVATE SDL2main)
    endif()
    # Copy SDL2.dll to output directory if using shared SDL2
    if(TARGET SDL2::SDL2)
        add_custom_command(TARGET veloce POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:veloce>
            COMMAND_EXPAND_LISTS
        )
    endif()
endif()

# Copy assets to build directory (only if assets directory exists)
if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
    add_custom_command(TARGET veloce POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    )
endif()

# Emulator cores (cores/ directory)
add_subdirectory(cores/nes)
add_subdirectory(cores/snes)
add_subdirectory(cores/gb)
add_subdirectory(cores/gba)

# Auxiliary plugins (plugins/ directory)
add_subdirectory(plugins/audio_default)
add_subdirectory(plugins/input_default)
add_subdirectory(plugins/tas_default)

# Install rules
install(TARGETS veloce RUNTIME DESTINATION bin)
if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
    install(DIRECTORY assets/ DESTINATION share/veloce/assets)
endif()
