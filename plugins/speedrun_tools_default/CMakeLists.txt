# Default Game Plugin - Built-in Timer
# Provides built-in timer, splits tracking, and PB management
# Implements the unified IGamePlugin interface
add_library(game_timer_default SHARED
    src/default_speedrun_tools_plugin.cpp
)

target_include_directories(game_timer_default PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(game_timer_default PRIVATE
    game_plugin_common
    imgui
)

set_target_properties(game_timer_default PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
)

# For multi-config generators (Visual Studio, Xcode)
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(game_timer_default PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
    )
endforeach()
