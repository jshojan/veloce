# Game Plugin Common Library
# Shared components for game plugins including timer logic, splits file I/O, and GUI panels

add_library(game_plugin_common STATIC
    src/timer_core.cpp
    src/splits_file.cpp
    src/panels/timer_panel.cpp
)

target_include_directories(game_plugin_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(game_plugin_common PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(game_plugin_common PUBLIC
    nlohmann_json::nlohmann_json
)

target_link_libraries(game_plugin_common PRIVATE
    imgui
)

# Compile with position-independent code since this will be linked into shared libraries
set_target_properties(game_plugin_common PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
