foreach(profile IN ITEMS gc wii)
    add_library(${profile}_plugin SHARED src/plugin.cpp src/machine.cpp src/cpu.cpp)
    target_include_directories(${profile}_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_compile_features(${profile}_plugin PRIVATE cxx_std_17)
    if(profile STREQUAL "wii")
        target_compile_definitions(${profile}_plugin PRIVATE PPC_PROFILE_WII)
    endif()
    set_target_properties(${profile}_plugin PROPERTIES PREFIX "" OUTPUT_NAME ${profile}
        LIBRARY_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY ${VELOCE_CORES_OUTPUT_DIRECTORY})
    foreach(config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${config} upper)
        set_target_properties(${profile}_plugin PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${upper} ${VELOCE_CORES_OUTPUT_DIRECTORY}
            RUNTIME_OUTPUT_DIRECTORY_${upper} ${VELOCE_CORES_OUTPUT_DIRECTORY})
    endforeach()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${profile}_plugin PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
