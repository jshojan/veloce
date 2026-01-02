# Default Audio Plugin
# Non-core plugins go to the plugins/ directory
add_library(audio_default SHARED
    src/default_audio_plugin.cpp
)

target_include_directories(audio_default PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(audio_default PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
)

# For multi-config generators (Visual Studio, Xcode)
foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG_TYPE} CONFIG_TYPE_UPPER)
    set_target_properties(audio_default PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_TYPE_UPPER} ${VELOCE_PLUGINS_OUTPUT_DIRECTORY}
    )
endforeach()
