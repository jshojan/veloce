# Default Input Plugin
# Non-core plugins go to the plugins/ directory
add_library(input_default SHARED
    src/default_input_plugin.cpp
)

target_include_directories(input_default PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(input_default PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
)

# For multi-config generators (Visual Studio, Xcode)
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(input_default PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
    )
endforeach()
