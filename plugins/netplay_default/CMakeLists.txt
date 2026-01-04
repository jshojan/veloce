# Default Netplay Plugin
# Provides rollback netplay with GUI integration

add_library(netplay_default SHARED
    src/default_netplay_plugin.cpp
    src/netplay_input_manager.cpp
)

target_include_directories(netplay_default PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(netplay_default PRIVATE
    imgui
    nlohmann_json::nlohmann_json
)

# Set output directory
set_target_properties(netplay_default PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
)

# Copy to build directory for testing
add_custom_command(TARGET netplay_default POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:netplay_default>
        "${CMAKE_BINARY_DIR}/bin/plugins/$<TARGET_FILE_NAME:netplay_default>"
)
