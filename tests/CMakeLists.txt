# Veloce test + accuracy framework - CTest integration.
#
# Included from the root CMakeLists via add_subdirectory(tests) when
# BUILD_TESTING is on (the default when include(CTest) is active).
#
# Targets registered:
#   testkit_selftest    - fast pure-logic unit test (no ROMs, no emulator)
#   rom_build_selftest  - fast unit test of build.py's Recipe/local_source logic (no docker)
#   test_file_sink      - C++ unit test of the VELOCE_TEST_OUT sink (stub plugin)
#   nes_test_channel    - C++ unit test of the NES $401E port + blargg6000 adapter
#   gb_test_channel     - C++ unit test of the Game Boy result channel + adapters
#   validate_configs    - schema validation of every test_config.json
#   accuracy_<console>  - full per-console accuracy run (needs network for ROMs)
#   accuracy_all        - cross-console aggregate scorecard
#
# Run:  ctest --test-dir build                 # fast gates (selftest+validate)
#       ctest --test-dir build -L accuracy      # full accuracy suites (slow)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(n64_machine_test cpp/n64_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src
    ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_machine_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_machine_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_machine_test n64_plugin)
add_test(NAME n64_machine_test COMMAND n64_machine_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/integer_registers/integer_registers.z64
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_serial/controller_serial.z64)
set_tests_properties(n64_machine_test PROPERTIES LABELS "unit;fast" TIMEOUT 30)
add_test(NAME n64_integer_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/integer_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_integer)
set_tests_properties(n64_integer_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 150)
add_test(NAME n64_muldiv64 COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/muldiv64/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_muldiv64)
set_tests_properties(n64_muldiv64 PROPERTIES LABELS "accuracy;owned" TIMEOUT 800)
add_test(NAME n64_merge_memory COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/merge_memory/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_merge_memory)
set_tests_properties(n64_merge_memory PROPERTIES LABELS "accuracy;owned" TIMEOUT 800)

add_test(NAME n64_controller_serial COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_serial/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_controller)
set_tests_properties(n64_controller_serial PROPERTIES LABELS "accuracy;owned" TIMEOUT 180)

add_test(NAME n64_interrupts COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/interrupts/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_interrupts)
set_tests_properties(n64_interrupts PROPERTIES LABELS "accuracy;owned" TIMEOUT 250)

add_executable(vb_machine_test cpp/vb_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/vb/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/vb/src/v810.cpp)
target_include_directories(vb_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/vb/src
    ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(vb_machine_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(vb_machine_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(vb_machine_test vb_plugin)
add_test(NAME vb_machine_test COMMAND vb_machine_test $<TARGET_FILE:vb_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/integer_registers/integer_registers.vb)
set_tests_properties(vb_machine_test PROPERTIES LABELS "unit;fast" TIMEOUT 30)
add_test(NAME vb_integer_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/integer_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/vb_integer)
set_tests_properties(vb_integer_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME vb_muldiv COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/muldiv/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/vb_muldiv)
set_tests_properties(vb_muldiv PROPERTIES LABELS "accuracy;owned" TIMEOUT 1000)
add_test(NAME vb_exceptions COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/exceptions/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/vb_exceptions)
set_tests_properties(vb_exceptions PROPERTIES LABELS "accuracy;owned" TIMEOUT 1000)
add_test(NAME vb_address_traps COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/address_traps/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/vb_address_traps)
set_tests_properties(vb_address_traps PROPERTIES LABELS "accuracy;owned" TIMEOUT 1000)
add_test(NAME vb_memory_ops COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/memory_ops/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/vb_memory_ops)
set_tests_properties(vb_memory_ops PROPERTIES LABELS "accuracy;owned" TIMEOUT 1400)
add_test(NAME vb_bitstrings COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/bitstrings/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/vb_bitstrings)
set_tests_properties(vb_bitstrings PROPERTIES LABELS "accuracy;owned" TIMEOUT 2100)
add_test(NAME vb_immediates COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/vb/immediates/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/vb_immediates)
set_tests_properties(vb_immediates PROPERTIES LABELS "accuracy;owned" TIMEOUT 3500)

add_executable(snes_math_pipeline_test cpp/snes_math_pipeline_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/snes/src/test_channel.cpp
    ${CMAKE_SOURCE_DIR}/cores/snes/src/bus.cpp ${CMAKE_SOURCE_DIR}/cores/snes/src/cpu.cpp
    ${CMAKE_SOURCE_DIR}/cores/snes/src/ppu.cpp ${CMAKE_SOURCE_DIR}/cores/snes/src/apu.cpp
    ${CMAKE_SOURCE_DIR}/cores/snes/src/dsp.cpp ${CMAKE_SOURCE_DIR}/cores/snes/src/spc700.cpp
    ${CMAKE_SOURCE_DIR}/cores/snes/src/dma.cpp ${CMAKE_SOURCE_DIR}/cores/snes/src/cartridge.cpp)
target_include_directories(snes_math_pipeline_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/snes/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(snes_math_pipeline_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(snes_math_pipeline_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(snes_math_pipeline_test snes_plugin)
add_test(NAME snes_math_pipeline_test COMMAND snes_math_pipeline_test $<TARGET_FILE:snes_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/snes/math_timing/math_timing.sfc)
set_tests_properties(snes_math_pipeline_test PROPERTIES LABELS "unit;fast" TIMEOUT 30)

add_test(NAME snes_math_timing COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/snes/math_timing/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/snes_math_timing)
set_tests_properties(snes_math_timing PROPERTIES LABELS "accuracy;owned" TIMEOUT 400)

add_executable(netplay_plugin_peer cpp/netplay_plugin_peer.cpp)
target_include_directories(netplay_plugin_peer PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(netplay_plugin_peer PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(netplay_plugin_peer PRIVATE SDL_MAIN_HANDLED)
add_dependencies(netplay_plugin_peer nes_plugin nds_plugin dsi_plugin netplay_default)
add_test(NAME netplay_plugin_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/netplay_network_test.py $<TARGET_FILE:netplay_plugin_peer>
    $<TARGET_FILE:nes_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/nes/netplay_probe/netplay_probe.nes
    ${CMAKE_BINARY_DIR}/artifacts/netplay-plugin --netplay-plugin $<TARGET_FILE:netplay_default>)
set_tests_properties(netplay_plugin_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 90)

add_executable(netplay_network_peer cpp/netplay_network_peer.cpp
    ${CMAKE_SOURCE_DIR}/plugins/netplay_default/src/rollback_session.cpp)
target_include_directories(netplay_network_peer PRIVATE ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/plugins/netplay_default/src ${SDL2_INCLUDE_DIRS})
target_link_libraries(netplay_network_peer PRIVATE ${SDL2_LIBRARIES} netplay_transport)
target_compile_definitions(netplay_network_peer PRIVATE SDL_MAIN_HANDLED)
add_dependencies(netplay_network_peer nes_plugin)
add_test(NAME netplay_network_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/netplay_network_test.py $<TARGET_FILE:netplay_network_peer>
    $<TARGET_FILE:nes_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/nes/netplay_probe/netplay_probe.nes
    ${CMAKE_BINARY_DIR}/artifacts/network)
set_tests_properties(netplay_network_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 90)

add_executable(netplay_rollback_test cpp/netplay_rollback_test.cpp
    ${CMAKE_SOURCE_DIR}/plugins/netplay_default/src/rollback_session.cpp)
target_include_directories(netplay_rollback_test PRIVATE ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/plugins/netplay_default/src ${SDL2_INCLUDE_DIRS})
target_link_libraries(netplay_rollback_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(netplay_rollback_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(netplay_rollback_test nes_plugin)
add_test(NAME netplay_rollback_test COMMAND netplay_rollback_test
    $<TARGET_FILE:nes_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nes/netplay_probe/netplay_probe.nes
    ${CMAKE_BINARY_DIR}/artifacts/rollback)
set_tests_properties(netplay_rollback_test PROPERTIES LABELS "integration;netplay;owned"
    TIMEOUT 60)

add_executable(input_hotplug_test cpp/input_hotplug_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/input_manager.cpp)
target_include_directories(input_hotplug_test PRIVATE ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src ${SDL2_INCLUDE_DIRS})
target_link_libraries(input_hotplug_test PRIVATE ${SDL2_LIBRARIES} nlohmann_json::nlohmann_json input_default)
target_compile_definitions(input_hotplug_test PRIVATE SDL_MAIN_HANDLED)
add_test(NAME input_hotplug_test COMMAND input_hotplug_test)
set_tests_properties(input_hotplug_test PROPERTIES LABELS "integration;fast;controllers"
    TIMEOUT 30 SKIP_RETURN_CODE 77)

set(VELOCE_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# --- fast gates (always run, no emulator/ROMs needed) ---------------------
add_test(
    NAME testkit_selftest
    COMMAND ${Python3_EXECUTABLE} ${VELOCE_TESTS_DIR}/veloce_testkit/selftest.py
)
set_tests_properties(testkit_selftest PROPERTIES LABELS "unit;fast")

add_test(
    NAME validate_configs
    COMMAND ${Python3_EXECUTABLE} ${VELOCE_TESTS_DIR}/validate_configs.py
)
set_tests_properties(validate_configs PROPERTIES LABELS "unit;fast")

add_test(
    NAME rom_build_selftest
    COMMAND ${Python3_EXECUTABLE} ${VELOCE_TESTS_DIR}/roms-src/selftest.py
)
set_tests_properties(rom_build_selftest PROPERTIES LABELS "unit;fast")

# Test-result sink (plugin ABI v2): TestFileSink driven by a stub plugin.
add_executable(test_file_sink
    ${VELOCE_TESTS_DIR}/cpp/test_file_sink_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/test_file_sink.cpp
)
target_include_directories(test_file_sink PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME test_file_sink COMMAND test_file_sink)
set_tests_properties(test_file_sink PROPERTIES LABELS "unit;fast")

add_executable(cartridge_crc_test
    cpp/cartridge_crc_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/cartridge.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc1.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc3.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc5.cpp
    ${CMAKE_SOURCE_DIR}/cores/gba/src/cartridge.cpp
)
target_include_directories(cartridge_crc_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME cartridge_crc_test COMMAND cartridge_crc_test)
set_tests_properties(cartridge_crc_test PROPERTIES LABELS "unit;fast")

add_test(NAME original_rom_builds
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/roms-src/build.py --original-only --check)
set_tests_properties(original_rom_builds PROPERTIES LABELS "unit;fast")

# Original checked-in Game Boy ROM: validates serial register observations,
# the file sink, and an injected failure through the real headless frontend.
add_test(
    NAME gb_register_probe
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/roms-src/gb/register_probe/test_probe.py
            $<TARGET_FILE:veloce>
            --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/gb_register_probe
)
set_tests_properties(gb_register_probe PROPERTIES LABELS "accuracy;fast;owned"
    REQUIRED_FILES $<TARGET_FILE:veloce>)

add_test(NAME gb_alu_exhaustive COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/gb/alu_exhaustive/run.py
    --emulator $<TARGET_FILE:veloce> --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/gb_alu)
set_tests_properties(gb_alu_exhaustive PROPERTIES LABELS "accuracy;owned" TIMEOUT 950
    REQUIRED_FILES $<TARGET_FILE:veloce>)

add_test(NAME gb_unary_exhaustive COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/gb/unary_exhaustive/run.py
    --emulator $<TARGET_FILE:veloce> --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/gb_unary)
set_tests_properties(gb_unary_exhaustive PROPERTIES LABELS "accuracy;owned" TIMEOUT 150
    REQUIRED_FILES $<TARGET_FILE:veloce>)

add_test(
    NAME nes_alu_exhaustive
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/roms-src/nes/alu_exhaustive/run.py
            --emulator $<TARGET_FILE:veloce>
            --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nes_alu
)
set_tests_properties(nes_alu_exhaustive PROPERTIES LABELS "accuracy;owned"
    TIMEOUT 240 REQUIRED_FILES $<TARGET_FILE:veloce>)

add_test(NAME snes_math_registers
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/roms-src/snes/math_registers/run.py
            --emulator $<TARGET_FILE:veloce>
            --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/snes_math)
set_tests_properties(snes_math_registers PROPERTIES LABELS "accuracy;owned"
    TIMEOUT 240 REQUIRED_FILES $<TARGET_FILE:veloce>)

add_test(NAME gba_arm_shifts
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/roms-src/gba/arm_shifts/run.py
            --emulator $<TARGET_FILE:veloce>
            --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/gba_shifts)
set_tests_properties(gba_arm_shifts PROPERTIES LABELS "accuracy;owned"
    TIMEOUT 240 REQUIRED_FILES $<TARGET_FILE:veloce>)
# NES test channel: $401E/$401F port + blargg6000 adapter against a recording sink.
add_executable(nes_test_channel_test
    ${VELOCE_TESTS_DIR}/cpp/nes_test_channel_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/nes/src/test_channel.cpp
)
target_include_directories(nes_test_channel_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cores/nes/src
)
add_test(NAME nes_test_channel COMMAND nes_test_channel_test)
set_tests_properties(nes_test_channel PROPERTIES LABELS "unit;fast")
# Game Boy result channel (gb::TestChannel): serial + mooneye/a000/hram/blarggtext
# adapters against a recording sink.
add_executable(gb_test_channel
    ${VELOCE_TESTS_DIR}/cpp/gb_test_channel_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/test_channel.cpp
)
target_include_directories(gb_test_channel PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cores/gb/src
)
add_test(NAME gb_test_channel COMMAND gb_test_channel)
set_tests_properties(gb_test_channel PROPERTIES LABELS "unit;fast")

# --- per-console accuracy suites (slow: download ROMs, run emulator) ------
foreach(console nes snes gb gba)
    if(console STREQUAL "nes")
        set(console_runner runner.py)
    elseif(console STREQUAL "gb")
        set(console_runner run_tests.py)
    else()
        set(console_runner test_runner.py)
    endif()
    add_test(
        NAME accuracy_${console}
        COMMAND ${Python3_EXECUTABLE}
                ${VELOCE_TESTS_DIR}/../cores/${console}/tests/${console_runner} --json
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(accuracy_${console} PROPERTIES
        LABELS "accuracy;slow"
        TIMEOUT 1800
        # depends on the veloce binary existing
        REQUIRED_FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/veloce
    )
endforeach()

# --- aggregate scorecard --------------------------------------------------
add_test(
    NAME accuracy_all
    COMMAND ${Python3_EXECUTABLE} ${VELOCE_TESTS_DIR}/run_all.py --json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(accuracy_all PROPERTIES LABELS "accuracy;slow" TIMEOUT 7200)

add_executable(nds_machine_test cpp/nds_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/nds/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/nds/src/arm.cpp)
target_include_directories(nds_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/nds/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(nds_machine_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(nds_machine_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(nds_machine_test nds_plugin)
add_test(NAME nds_machine_test COMMAND nds_machine_test $<TARGET_FILE:nds_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/integer_registers/integer_registers.nds)
set_tests_properties(nds_machine_test PROPERTIES LABELS "unit;fast" TIMEOUT 30)
add_test(NAME nds_integer_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/integer_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nds_integer)
set_tests_properties(nds_integer_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1800)
add_test(NAME nds_multiply_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/multiply_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nds_multiply)
set_tests_properties(nds_multiply_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1800)
add_test(NAME nds_thumb_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/thumb_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nds_thumb)
set_tests_properties(nds_thumb_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 2400)

add_test(NAME nds_memory_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/memory_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nds_memory)
set_tests_properties(nds_memory_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 3600)

add_test(NAME nds_long_calls COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/long_calls/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nds_long_calls)
set_tests_properties(nds_long_calls PROPERTIES LABELS "accuracy;owned" TIMEOUT 1800)

add_test(NAME nds_multiple_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/multiple_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nds_multiple)
set_tests_properties(nds_multiple_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1800)

add_test(NAME nds_keypad COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/keypad/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nds_keypad)
set_tests_properties(nds_keypad PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)

add_test(NAME nds_netplay_plugin_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/netplay_network_test.py $<TARGET_FILE:netplay_plugin_peer>
    $<TARGET_FILE:nds_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/nds/netplay_probe/netplay_probe.nds
    ${CMAKE_BINARY_DIR}/artifacts/netplay-nds --netplay-plugin $<TARGET_FILE:netplay_default> --console nds)
set_tests_properties(nds_netplay_plugin_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 90)

add_executable(ppc_machine_test cpp/ppc_machine_test.cpp ${CMAKE_SOURCE_DIR}/cores/ppc/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/ppc/src/cpu.cpp ${CMAKE_SOURCE_DIR}/cores/ppc/src/serial.cpp)
target_include_directories(ppc_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/ppc/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(ppc_machine_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(ppc_machine_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(ppc_machine_test gc_plugin wii_plugin)
add_test(NAME ppc_machine_test COMMAND ppc_machine_test $<TARGET_FILE:gc_plugin> $<TARGET_FILE:wii_plugin>)
set_tests_properties(ppc_machine_test PROPERTIES LABELS "unit;fast" TIMEOUT 90)
foreach(profile gc wii)
    add_test(NAME ${profile}_integer_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/integer_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_integer)
    set_tests_properties(${profile}_integer_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 150)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_branch_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/branch_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_branches)
    set_tests_properties(${profile}_branch_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 150)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_condition_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/condition_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_conditions)
    set_tests_properties(${profile}_condition_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 2400)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_logical_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/logical_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_logical)
    set_tests_properties(${profile}_logical_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 600)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_rotate_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/rotate_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_rotates)
    set_tests_properties(${profile}_rotate_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 600)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_shift_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/shift_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_shifts)
    set_tests_properties(${profile}_shift_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 600)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_carry_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/carry_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_carry)
    set_tests_properties(${profile}_carry_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1200)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_multiply_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/multiply_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_products)
    set_tests_properties(${profile}_multiply_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1200)
endforeach()

foreach(profile gc wii)
    add_test(NAME ${profile}_memory_registers COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/memory_registers/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_memory)
    set_tests_properties(${profile}_memory_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1200)
endforeach()

add_executable(dsi_machine_test cpp/dsi_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/nds/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/nds/src/arm.cpp)
target_include_directories(dsi_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/nds/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(dsi_machine_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(dsi_machine_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(dsi_machine_test nds_plugin dsi_plugin)
add_test(NAME dsi_machine_test COMMAND dsi_machine_test $<TARGET_FILE:dsi_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/dsi/netplay_probe/netplay_probe.vdsi $<TARGET_FILE:nds_plugin>)
set_tests_properties(dsi_machine_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 90)
add_test(NAME dsi_ram_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/dsi/ram_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/dsi_ram)
set_tests_properties(dsi_ram_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 600)
add_test(NAME dsi_netplay_plugin_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/netplay_network_test.py $<TARGET_FILE:netplay_plugin_peer>
    $<TARGET_FILE:dsi_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/dsi/netplay_probe/netplay_probe.vdsi
    ${CMAKE_BINARY_DIR}/artifacts/netplay-dsi --netplay-plugin $<TARGET_FILE:netplay_default> --console dsi)
set_tests_properties(dsi_netplay_plugin_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 120)

add_executable(ctr_machine_test cpp/ctr_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/3ds/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/nds/src/arm.cpp)
target_include_directories(ctr_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/3ds/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(ctr_machine_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(ctr_machine_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(ctr_machine_test ctr_plugin)
add_test(NAME ctr_machine_test COMMAND ctr_machine_test $<TARGET_FILE:ctr_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/3ds/reversal_registers/reversal_registers.v3ds)
set_tests_properties(ctr_machine_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME ctr_reversal_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/3ds/reversal_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/3ds_reversal)
set_tests_properties(ctr_reversal_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1800)
add_test(NAME ctr_netplay_plugin_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/netplay_network_test.py $<TARGET_FILE:netplay_plugin_peer>
    $<TARGET_FILE:ctr_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/3ds/netplay_probe/netplay_probe.v3ds
    ${CMAKE_BINARY_DIR}/artifacts/netplay-3ds --netplay-plugin $<TARGET_FILE:netplay_default> --console 3ds)
set_tests_properties(ctr_netplay_plugin_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 180)

add_executable(wiiu_machine_test cpp/wiiu_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/wiiu/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/ppc/src/cpu.cpp)
target_include_directories(wiiu_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/wiiu/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(wiiu_machine_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(wiiu_machine_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(wiiu_machine_test wiiu_plugin)
add_test(NAME wiiu_machine_test COMMAND wiiu_machine_test $<TARGET_FILE:wiiu_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/wiiu/immediate_registers/immediate_registers.vwiiu)
set_tests_properties(wiiu_machine_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 1200)
add_test(NAME wiiu_immediate_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/wiiu/immediate_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/wiiu_immediates)
set_tests_properties(wiiu_immediate_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 1800)
add_test(NAME wiiu_netplay_plugin_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/netplay_network_test.py $<TARGET_FILE:netplay_plugin_peer>
    $<TARGET_FILE:wiiu_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/wiiu/netplay_probe/netplay_probe.vwiiu
    ${CMAKE_BINARY_DIR}/artifacts/netplay-wiiu --netplay-plugin $<TARGET_FILE:netplay_default> --console wiiu)
set_tests_properties(wiiu_netplay_plugin_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 180)

add_executable(ppc_serial_test cpp/ppc_serial_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/ppc/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/ppc/src/cpu.cpp
    ${CMAKE_SOURCE_DIR}/cores/ppc/src/serial.cpp)
target_include_directories(ppc_serial_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/ppc/src
    ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(ppc_serial_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(ppc_serial_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(ppc_serial_test gc_plugin wii_plugin)
add_test(NAME ppc_serial_test COMMAND ppc_serial_test $<TARGET_FILE:gc_plugin> $<TARGET_FILE:wii_plugin>)
set_tests_properties(ppc_serial_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 90)
foreach(profile gc wii)
    add_test(NAME ${profile}_controller_serial COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/${profile}/controller_serial/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/${profile}_controller)
    set_tests_properties(${profile}_controller_serial PROPERTIES LABELS "accuracy;owned" TIMEOUT 180)
endforeach()

# Optional local commercial-ROM probe. No ROM path or bytes enter CTest/git.
add_executable(retail_core_probe cpp/retail_core_probe.cpp)
target_include_directories(retail_core_probe PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(retail_core_probe PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(retail_core_probe PRIVATE SDL_MAIN_HANDLED)

add_executable(n64_boot_trace cpp/n64_boot_trace.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_boot_trace PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)

add_executable(n64_pi_test cpp/n64_pi_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_pi_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_pi_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_pi_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_pi_test n64_plugin)
add_test(NAME n64_pi_test COMMAND n64_pi_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/integer_registers/integer_registers.z64
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/ipl3_handoff/ipl3_handoff.z64)
set_tests_properties(n64_pi_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
foreach(suite pi_dma ipl3_handoff ri_registers mi_controls rdram_devices cache_coherence sp_controls sp_dma ai_fifo ai_pcm fpu_controls tlb_translation vi_registers vi_framebuffer fpu_data fpu_integer_conversions fpu_conversion_edges fpu_arithmetic_pairs fpu_arithmetic_edges rsp_scalar_alu rsp_scalar_immediates dp_registers dp_fill_rectangles rdp_combiner_commands rdp_fill_pipeline rsp_vector_loads rsp_vector_stores rsp_vector_transfers rsp_multiply_pairs rsp_accumulate_edges rsp_comparison_pairs rsp_comparison_edges rsp_integer_pairs rsp_integer_edges fpu_comparison_pairs fpu_comparison_edges)
    add_test(NAME n64_${suite} COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/${suite}/run.py --emulator $<TARGET_FILE:veloce>
        --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_${suite})
    set_tests_properties(n64_${suite} PROPERTIES LABELS "accuracy;owned" TIMEOUT 900)
endforeach()

add_executable(n64_ri_test cpp/n64_ri_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_ri_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_ri_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_ri_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_ri_test n64_plugin)
add_test(NAME n64_ri_test COMMAND n64_ri_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/ri_registers/ri_registers.z64)
set_tests_properties(n64_ri_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_mi_test cpp/n64_mi_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_mi_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_mi_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_mi_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_mi_test n64_plugin)
add_test(NAME n64_mi_test COMMAND n64_mi_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/mi_controls/mi_controls.z64)
set_tests_properties(n64_mi_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_rdram_test cpp/n64_rdram_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdram_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_rdram_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_rdram_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_rdram_test n64_plugin)
add_test(NAME n64_rdram_test COMMAND n64_rdram_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdram_devices/rdram_devices.z64)
set_tests_properties(n64_rdram_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_cache_test cpp/n64_cache_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_cache_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_cache_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_cache_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_cache_test n64_plugin)
add_test(NAME n64_cache_test COMMAND n64_cache_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/cache_coherence/cache_coherence.z64)
set_tests_properties(n64_cache_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_sp_test cpp/n64_sp_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_sp_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_sp_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_sp_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_sp_test n64_plugin)
add_test(NAME n64_sp_test COMMAND n64_sp_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/sp_controls/sp_controls.z64)
set_tests_properties(n64_sp_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_audio_capture cpp/n64_audio_capture.cpp)
target_include_directories(n64_audio_capture PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_audio_capture PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_audio_capture PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_audio_capture n64_plugin)
add_test(NAME n64_ai_pcm_audio COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/ai_pcm/audio.py --capture $<TARGET_FILE:n64_audio_capture>
    --core $<TARGET_FILE:n64_plugin> --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_ai_pcm_audio)
set_tests_properties(n64_ai_pcm_audio PROPERTIES LABELS "accuracy;owned" TIMEOUT 600)
add_executable(n64_ai_test cpp/n64_ai_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_ai_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_ai_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_ai_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_ai_test n64_plugin)
add_test(NAME n64_ai_test COMMAND n64_ai_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/ai_pcm/ai_pcm.z64)
set_tests_properties(n64_ai_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_fpu_controls_test cpp/n64_fpu_controls_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_fpu_controls_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_fpu_controls_test COMMAND n64_fpu_controls_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/fpu_controls/fpu_controls.z64)
set_tests_properties(n64_fpu_controls_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_tlb_test cpp/n64_tlb_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_tlb_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_tlb_test COMMAND n64_tlb_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/tlb_translation/tlb_translation.z64)
set_tests_properties(n64_tlb_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_video_capture cpp/n64_video_capture.cpp)
target_include_directories(n64_video_capture PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_video_capture PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_video_capture PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_video_capture n64_plugin)
add_test(NAME n64_vi_video COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/vi_framebuffer/video.py --capture $<TARGET_FILE:n64_video_capture>
    --core $<TARGET_FILE:n64_plugin> --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_vi_video)
set_tests_properties(n64_vi_video PROPERTIES LABELS "accuracy;owned" TIMEOUT 300)
add_executable(n64_vi_test cpp/n64_vi_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_vi_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_vi_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(n64_vi_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(n64_vi_test n64_plugin)
add_test(NAME n64_vi_test COMMAND n64_vi_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/vi_framebuffer/vi_framebuffer.z64)
set_tests_properties(n64_vi_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_fpu_data_test cpp/n64_fpu_data_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_fpu_data_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_fpu_data_test COMMAND n64_fpu_data_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/fpu_data/fpu_data.z64)
set_tests_properties(n64_fpu_data_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_fpu_conversion_test cpp/n64_fpu_conversion_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_fpu_conversion_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_fpu_conversion_test COMMAND n64_fpu_conversion_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/fpu_integer_conversions/fpu_integer_conversions.z64)
set_tests_properties(n64_fpu_conversion_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_fpu_arithmetic_test cpp/n64_fpu_arithmetic_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_fpu_arithmetic_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_fpu_arithmetic_test COMMAND n64_fpu_arithmetic_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/fpu_arithmetic_pairs/fpu_arithmetic_pairs.z64)
set_tests_properties(n64_fpu_arithmetic_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_rsp_scalar_test cpp/n64_rsp_scalar_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rsp_scalar_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rsp_scalar_test COMMAND n64_rsp_scalar_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rsp_scalar_alu/rsp_scalar_alu.z64)
set_tests_properties(n64_rsp_scalar_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_dp_test cpp/n64_dp_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_dp_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_dp_test COMMAND n64_dp_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/dp_registers/dp_registers.z64)
set_tests_properties(n64_dp_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_rsp_vector_test cpp/n64_rsp_vector_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rsp_vector_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rsp_vector_test COMMAND n64_rsp_vector_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rsp_vector_loads/rsp_vector_loads.z64)
set_tests_properties(n64_rsp_vector_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_rsp_multiply_test cpp/n64_rsp_multiply_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rsp_multiply_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rsp_multiply_test COMMAND n64_rsp_multiply_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rsp_vector_loads/rsp_vector_loads.z64)
set_tests_properties(n64_rsp_multiply_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_fpu_comparison_test cpp/n64_fpu_comparison_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_fpu_comparison_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_fpu_comparison_test COMMAND n64_fpu_comparison_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/fpu_comparison_pairs/fpu_comparison_pairs.z64)
set_tests_properties(n64_fpu_comparison_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_executable(n64_rdp_combiner_test cpp/n64_rdp_combiner_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_combiner_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_combiner_test COMMAND n64_rdp_combiner_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/dp_registers/dp_registers.z64)
set_tests_properties(n64_rdp_combiner_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_rdp_fill_test cpp/n64_rdp_fill_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_fill_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_fill_test COMMAND n64_rdp_fill_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/dp_registers/dp_registers.z64)
set_tests_properties(n64_rdp_fill_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_rsp_compare_test cpp/n64_rsp_compare_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rsp_compare_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rsp_compare_test COMMAND n64_rsp_compare_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rsp_comparison_pairs/rsp_comparison_pairs.z64)
set_tests_properties(n64_rsp_compare_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_executable(n64_rsp_alu_test cpp/n64_rsp_alu_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rsp_alu_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rsp_alu_test COMMAND n64_rsp_alu_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rsp_integer_pairs/rsp_integer_pairs.z64)
set_tests_properties(n64_rsp_alu_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

set_tests_properties(n64_rsp_integer_pairs n64_rsp_integer_edges PROPERTIES TIMEOUT 2100)

add_executable(n64_rsp_divider_test cpp/n64_rsp_divider_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rsp_divider_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rsp_divider_test COMMAND n64_rsp_divider_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rsp_divider/rsp_divider.z64)
set_tests_properties(n64_rsp_divider_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME n64_rsp_divider COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rsp_divider/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rsp_divider)
set_tests_properties(n64_rsp_divider PROPERTIES LABELS "accuracy;owned" TIMEOUT 2700)

add_executable(n64_rdp_texture_test cpp/n64_rdp_texture_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_texture_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_texture_test COMMAND n64_rdp_texture_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_combiner_commands/rdp_combiner_commands.z64)
set_tests_properties(n64_rdp_texture_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_test(NAME n64_rdp_tile_commands COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_tile_commands/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_tile_commands)
set_tests_properties(n64_rdp_tile_commands PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)

add_executable(n64_rdp_tmem_test cpp/n64_rdp_tmem_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_tmem_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_tmem_test COMMAND n64_rdp_tmem_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_tile_commands/rdp_tile_commands.z64)
set_tests_properties(n64_rdp_tmem_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 240)

add_test(NAME n64_rdp_tmem_commands COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_tmem_commands/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_tmem_commands)
set_tests_properties(n64_rdp_tmem_commands PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)

add_executable(n64_rdp_combiner_math_test cpp/n64_rdp_combiner_math_test.cpp)
target_include_directories(n64_rdp_combiner_math_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_combiner_math_test COMMAND n64_rdp_combiner_math_test)
set_tests_properties(n64_rdp_combiner_math_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 120)

add_executable(n64_controller_ports_test cpp/n64_controller_ports_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_controller_ports_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_controller_ports_test COMMAND n64_controller_ports_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_tile_commands/rdp_tile_commands.z64)
set_tests_properties(n64_controller_ports_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 120)

add_executable(n64_controller_probe cpp/n64_controller_probe.cpp)
target_include_directories(n64_controller_probe PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_controller_probe PRIVATE ${SDL2_LIBRARIES})
add_dependencies(n64_controller_probe n64_plugin)
add_test(NAME n64_controller_netplay_guest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_netplay/run.py --probe $<TARGET_FILE:n64_controller_probe>
    --core $<TARGET_FILE:n64_plugin> --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_controller_netplay)
set_tests_properties(n64_controller_netplay_guest PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)

add_executable(n64_netplay_state_test cpp/n64_netplay_state_test.cpp)
target_include_directories(n64_netplay_state_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_netplay_state_test PRIVATE ${SDL2_LIBRARIES})
add_dependencies(n64_netplay_state_test n64_plugin)
add_test(NAME n64_netplay_state_test COMMAND n64_netplay_state_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_netplay/controller_netplay.z64)
set_tests_properties(n64_netplay_state_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 90)

add_dependencies(netplay_plugin_peer n64_plugin)
add_test(NAME n64_netplay_plugin_test COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/netplay_network_test.py $<TARGET_FILE:netplay_plugin_peer>
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_netplay/controller_netplay.z64
    ${CMAKE_BINARY_DIR}/artifacts/netplay-n64 --netplay-plugin $<TARGET_FILE:netplay_default> --console n64)
set_tests_properties(n64_netplay_plugin_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 180)

add_executable(switch_machine_test cpp/switch_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/switch/src/aarch64.cpp ${CMAKE_SOURCE_DIR}/cores/switch/src/machine.cpp)
target_include_directories(switch_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/cores/switch/src)
target_compile_features(switch_machine_test PRIVATE cxx_std_17)
add_test(NAME switch_machine_test COMMAND switch_machine_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/arithmetic_registers/arithmetic_registers.nso)
set_tests_properties(switch_machine_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME switch_arithmetic_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/arithmetic_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/switch_arithmetic)
set_tests_properties(switch_arithmetic_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)
add_executable(switch_plugin_state_test cpp/switch_plugin_state_test.cpp)
target_include_directories(switch_plugin_state_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(switch_plugin_state_test PRIVATE ${SDL2_LIBRARIES})
target_compile_definitions(switch_plugin_state_test PRIVATE SDL_MAIN_HANDLED)
add_dependencies(switch_plugin_state_test switch_plugin)
add_test(NAME switch_plugin_state_test COMMAND switch_plugin_state_test $<TARGET_FILE:switch_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/arithmetic_registers/arithmetic_registers.nso)
set_tests_properties(switch_plugin_state_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 90)

add_executable(switch_muldiv_test cpp/switch_muldiv_test.cpp ${CMAKE_SOURCE_DIR}/cores/switch/src/aarch64.cpp)
target_include_directories(switch_muldiv_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/switch/src)
target_compile_features(switch_muldiv_test PRIVATE cxx_std_17)
add_test(NAME switch_muldiv_test COMMAND switch_muldiv_test)
set_tests_properties(switch_muldiv_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME switch_muldiv_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/muldiv_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/switch_muldiv)
set_tests_properties(switch_muldiv_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)
add_test(NAME switch_muldiv_plugin_state_test COMMAND switch_plugin_state_test $<TARGET_FILE:switch_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/muldiv_registers/muldiv_registers.nso)
set_tests_properties(switch_muldiv_plugin_state_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 90)

add_executable(switch_select_test cpp/switch_select_test.cpp ${CMAKE_SOURCE_DIR}/cores/switch/src/aarch64.cpp)
target_include_directories(switch_select_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/switch/src)
target_compile_features(switch_select_test PRIVATE cxx_std_17)
add_test(NAME switch_select_test COMMAND switch_select_test)
set_tests_properties(switch_select_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME switch_select_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/select_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/switch_select)
set_tests_properties(switch_select_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)
add_test(NAME switch_select_plugin_state_test COMMAND switch_plugin_state_test $<TARGET_FILE:switch_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/select_registers/select_registers.nso)
set_tests_properties(switch_select_plugin_state_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 90)

add_executable(switch_bitops_test cpp/switch_bitops_test.cpp ${CMAKE_SOURCE_DIR}/cores/switch/src/aarch64.cpp)
target_include_directories(switch_bitops_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/switch/src)
target_compile_features(switch_bitops_test PRIVATE cxx_std_17)
add_test(NAME switch_bitops_test COMMAND switch_bitops_test)
set_tests_properties(switch_bitops_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME switch_bitops_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/bitops_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/switch_bitops)
set_tests_properties(switch_bitops_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)
add_test(NAME switch_bitops_plugin_state_test COMMAND switch_plugin_state_test $<TARGET_FILE:switch_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/bitops_registers/bitops_registers.nso)
set_tests_properties(switch_bitops_plugin_state_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 90)

add_executable(switch_bitfield_test cpp/switch_bitfield_test.cpp ${CMAKE_SOURCE_DIR}/cores/switch/src/aarch64.cpp)
target_include_directories(switch_bitfield_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/switch/src)
target_compile_features(switch_bitfield_test PRIVATE cxx_std_17)
add_test(NAME switch_bitfield_test COMMAND switch_bitfield_test)
set_tests_properties(switch_bitfield_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME switch_bitfield_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/bitfield_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/switch_bitfield)
set_tests_properties(switch_bitfield_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)
add_test(NAME switch_bitfield_plugin_state_test COMMAND switch_plugin_state_test $<TARGET_FILE:switch_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/bitfield_registers/bitfield_registers.nso)
set_tests_properties(switch_bitfield_plugin_state_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 90)

add_executable(switch_logical_imm_test cpp/switch_logical_imm_test.cpp ${CMAKE_SOURCE_DIR}/cores/switch/src/aarch64.cpp)
target_include_directories(switch_logical_imm_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/switch/src)
target_compile_features(switch_logical_imm_test PRIVATE cxx_std_17)
add_test(NAME switch_logical_imm_test COMMAND switch_logical_imm_test)
set_tests_properties(switch_logical_imm_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME switch_logical_imm_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/logical_imm_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/switch_logical_imm)
set_tests_properties(switch_logical_imm_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)
add_test(NAME switch_logical_imm_plugin_state_test COMMAND switch_plugin_state_test $<TARGET_FILE:switch_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/logical_imm_registers/logical_imm_registers.nso)
set_tests_properties(switch_logical_imm_plugin_state_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 90)

add_executable(switch_ccmp_test cpp/switch_ccmp_test.cpp ${CMAKE_SOURCE_DIR}/cores/switch/src/aarch64.cpp)
target_include_directories(switch_ccmp_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/switch/src)
target_compile_features(switch_ccmp_test PRIVATE cxx_std_17)
add_test(NAME switch_ccmp_test COMMAND switch_ccmp_test)
set_tests_properties(switch_ccmp_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME switch_ccmp_registers COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/ccmp_registers/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/switch_ccmp)
set_tests_properties(switch_ccmp_registers PROPERTIES LABELS "accuracy;owned" TIMEOUT 240)
add_test(NAME switch_ccmp_plugin_state_test COMMAND switch_plugin_state_test $<TARGET_FILE:switch_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/switch/ccmp_registers/ccmp_registers.nso)
set_tests_properties(switch_ccmp_plugin_state_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 90)

add_executable(n64_rdp_packet_test cpp/n64_rdp_packet_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_packet_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_packet_test COMMAND n64_rdp_packet_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/dp_registers/dp_registers.z64)
set_tests_properties(n64_rdp_packet_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_test(NAME n64_rdp_packets COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_packets/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_packets)
set_tests_properties(n64_rdp_packets PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)

add_executable(n64_rdp_packet_plugin_test cpp/n64_rdp_packet_plugin_test.cpp)
target_include_directories(n64_rdp_packet_plugin_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(n64_rdp_packet_plugin_test PRIVATE ${SDL2_LIBRARIES})
add_dependencies(n64_rdp_packet_plugin_test n64_plugin)
add_test(NAME n64_rdp_packet_plugin_test COMMAND n64_rdp_packet_plugin_test $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_packets/rdp_packets.z64)
set_tests_properties(n64_rdp_packet_plugin_test PROPERTIES LABELS "integration;netplay;owned" TIMEOUT 90)

foreach(triangle_part decode edges fill attributes shade)
    add_executable(n64_rdp_triangle_${triangle_part}_test cpp/n64_rdp_triangle_${triangle_part}_test.cpp)
    target_include_directories(n64_rdp_triangle_${triangle_part}_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
    add_test(NAME n64_rdp_triangle_${triangle_part}_test COMMAND n64_rdp_triangle_${triangle_part}_test)
    set_tests_properties(n64_rdp_triangle_${triangle_part}_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
endforeach()
add_executable(n64_rdp_coverage_test cpp/n64_rdp_coverage_test.cpp)
target_include_directories(n64_rdp_coverage_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_coverage_test COMMAND n64_rdp_coverage_test)
set_tests_properties(n64_rdp_coverage_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_executable(n64_rdp_combiner_routing_test cpp/n64_rdp_combiner_routing_test.cpp)
target_include_directories(n64_rdp_combiner_routing_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_combiner_routing_test COMMAND n64_rdp_combiner_routing_test)
set_tests_properties(n64_rdp_combiner_routing_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_executable(n64_rdp_texture_coordinates_test cpp/n64_rdp_texture_coordinates_test.cpp)
target_include_directories(n64_rdp_texture_coordinates_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_texture_coordinates_test COMMAND n64_rdp_texture_coordinates_test)
set_tests_properties(n64_rdp_texture_coordinates_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_texture_coordinate_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_texture_coordinate_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_texture_coordinates)
set_tests_properties(n64_rdp_texture_coordinate_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_executable(n64_rdp_texel_fetch_test cpp/n64_rdp_texel_fetch_test.cpp)
target_include_directories(n64_rdp_texel_fetch_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_texel_fetch_test COMMAND n64_rdp_texel_fetch_test)
set_tests_properties(n64_rdp_texel_fetch_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_texel_fetch_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_texel_fetch_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_texel_fetch)
set_tests_properties(n64_rdp_texel_fetch_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_executable(n64_rdp_texel_quad_test cpp/n64_rdp_texel_quad_test.cpp)
target_include_directories(n64_rdp_texel_quad_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_texel_quad_test COMMAND n64_rdp_texel_quad_test)
set_tests_properties(n64_rdp_texel_quad_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_texel_quad_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_texel_quad_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_texel_quad)
set_tests_properties(n64_rdp_texel_quad_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_executable(n64_rdp_texture_filter_test cpp/n64_rdp_texture_filter_test.cpp)
target_include_directories(n64_rdp_texture_filter_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_texture_filter_test COMMAND n64_rdp_texture_filter_test)
set_tests_properties(n64_rdp_texture_filter_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_texture_filter_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_texture_filter_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_texture_filter)
set_tests_properties(n64_rdp_texture_filter_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_test(NAME n64_rdp_triangle_attribute_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_triangle_attribute_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_triangle_attributes)
set_tests_properties(n64_rdp_triangle_attribute_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_test(NAME n64_rdp_triangle_fill COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_triangle_fill/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_triangle_fill)
set_tests_properties(n64_rdp_triangle_fill PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)

add_test(NAME n64_rdp_triangle_shade COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_triangle_shade/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_triangle_shade)
set_tests_properties(n64_rdp_triangle_shade PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)

add_executable(n64_rdp_triangle_texture_test cpp/n64_rdp_triangle_texture_test.cpp)
target_include_directories(n64_rdp_triangle_texture_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_triangle_texture_test COMMAND n64_rdp_triangle_texture_test)
set_tests_properties(n64_rdp_triangle_texture_test PROPERTIES LABELS "unit;accuracy")
add_test(NAME n64_rdp_triangle_texture COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_triangle_texture/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_triangle_texture)
set_tests_properties(n64_rdp_triangle_texture PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_texture_plugin_replay_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_triangle_texture/rdp_triangle_texture.z64 texture)
set_tests_properties(n64_rdp_texture_plugin_replay_test PROPERTIES LABELS "integration;netplay;accuracy")
add_executable(n64_rdp_convert_test cpp/n64_rdp_convert_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_convert_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_convert_test COMMAND n64_rdp_convert_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_triangle_texture/rdp_triangle_texture.z64)
set_tests_properties(n64_rdp_convert_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)

add_test(NAME n64_rdp_color_convert COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_color_convert/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_color_convert)
set_tests_properties(n64_rdp_color_convert PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_conversion_plugin_replay_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin>
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_color_convert/rdp_color_convert.z64 conversion)
set_tests_properties(n64_rdp_conversion_plugin_replay_test PROPERTIES LABELS "integration;netplay;accuracy")

add_executable(n64_rdp_texture_divider_test cpp/n64_rdp_texture_divider_test.cpp)
target_include_directories(n64_rdp_texture_divider_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_texture_divider_test COMMAND n64_rdp_texture_divider_test --exhaustive)
set_tests_properties(n64_rdp_texture_divider_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 900)
add_test(NAME n64_rdp_texture_divider_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_texture_divider_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_texture_divider)
set_tests_properties(n64_rdp_texture_divider_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_texture_perspective COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_texture_perspective/run.py
    --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_texture_perspective)
set_tests_properties(n64_rdp_texture_perspective PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_perspective_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_texture_perspective/rdp_texture_perspective.z64 perspective)
set_tests_properties(n64_rdp_perspective_checkpoint_test PROPERTIES LABELS "unit;accuracy;owned;netplay" TIMEOUT 60)
add_executable(n64_rdp_perspective_triangle_test cpp/n64_rdp_perspective_triangle_test.cpp)
target_include_directories(n64_rdp_perspective_triangle_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_perspective_triangle_test COMMAND n64_rdp_perspective_triangle_test)
set_tests_properties(n64_rdp_perspective_triangle_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 60)

add_executable(n64_rdp_blender_test cpp/n64_rdp_blender_test.cpp)
target_include_directories(n64_rdp_blender_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_blender_test COMMAND n64_rdp_blender_test)
set_tests_properties(n64_rdp_blender_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_executable(n64_rdp_blender_state_test cpp/n64_rdp_blender_state_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_blender_state_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_blender_state_test COMMAND n64_rdp_blender_state_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_one_cycle_blend/rdp_one_cycle_blend.z64)
set_tests_properties(n64_rdp_blender_state_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME n64_rdp_one_cycle_blend COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_one_cycle_blend/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_one_cycle_blend)
set_tests_properties(n64_rdp_one_cycle_blend PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_blender_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_one_cycle_blend/rdp_one_cycle_blend.z64 blender)
set_tests_properties(n64_rdp_blender_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)
add_test(NAME n64_rdp_blender_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_blender_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_blender)
set_tests_properties(n64_rdp_blender_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_executable(n64_hidden_rdram_test cpp/n64_hidden_rdram_test.cpp)
target_include_directories(n64_hidden_rdram_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_hidden_rdram_test COMMAND n64_hidden_rdram_test)
set_tests_properties(n64_hidden_rdram_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_hidden_rdram_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_hidden_rdram_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_hidden_rdram)
set_tests_properties(n64_hidden_rdram_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_executable(n64_hidden_machine_test cpp/n64_hidden_machine_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_hidden_machine_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_hidden_machine_test COMMAND n64_hidden_machine_test ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/hidden_rdram/hidden_rdram.z64)
set_tests_properties(n64_hidden_machine_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 180)
add_test(NAME n64_hidden_rdram_guest COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/hidden_rdram/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_hidden_rdram)
set_tests_properties(n64_hidden_rdram_guest PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_hidden_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/hidden_rdram/hidden_rdram.z64 hidden)
set_tests_properties(n64_hidden_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)
add_executable(n64_rdp_framebuffer_test cpp/n64_rdp_framebuffer_test.cpp)
target_include_directories(n64_rdp_framebuffer_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_framebuffer_test COMMAND n64_rdp_framebuffer_test)
set_tests_properties(n64_rdp_framebuffer_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_hidden_rgba16 COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_hidden_rgba16/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_hidden_rgba16)
set_tests_properties(n64_rdp_hidden_rgba16 PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_hidden_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_hidden_rgba16/rdp_hidden_rgba16.z64 hidden)
set_tests_properties(n64_rdp_hidden_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)
add_test(NAME n64_rdp_framebuffer_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_rdp_framebuffer_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_rdp_framebuffer)
set_tests_properties(n64_rdp_framebuffer_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_executable(n64_rdp_depth_test cpp/n64_rdp_depth_test.cpp)
target_include_directories(n64_rdp_depth_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_depth_test COMMAND n64_rdp_depth_test)
set_tests_properties(n64_rdp_depth_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_rejected_history COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_rejected_history/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_rejected_history)
set_tests_properties(n64_rdp_rejected_history PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_rejected_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_rejected_history/rdp_rejected_history.z64 hidden)
set_tests_properties(n64_rdp_rejected_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)
add_test(NAME n64_rdp_depth_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_rdp_depth_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_rdp_depth)
set_tests_properties(n64_rdp_depth_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)

add_executable(n64_rdp_depth_pipeline_test cpp/n64_rdp_depth_pipeline_test.cpp)
target_include_directories(n64_rdp_depth_pipeline_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_depth_pipeline_test COMMAND n64_rdp_depth_pipeline_test)
set_tests_properties(n64_rdp_depth_pipeline_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_depth_pipeline_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_rdp_depth_pipeline_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_rdp_depth_pipeline)
set_tests_properties(n64_rdp_depth_pipeline_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 240)
add_test(NAME n64_rdp_triangle_depth COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_triangle_depth/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_triangle_depth)
set_tests_properties(n64_rdp_triangle_depth PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_depth_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_triangle_depth/rdp_triangle_depth.z64 hidden)
set_tests_properties(n64_rdp_depth_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)

add_executable(n64_rdp_primitive_depth_test cpp/n64_rdp_primitive_depth_test.cpp)
target_include_directories(n64_rdp_primitive_depth_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_primitive_depth_test COMMAND n64_rdp_primitive_depth_test)
set_tests_properties(n64_rdp_primitive_depth_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_executable(n64_rdp_primitive_state_test cpp/n64_rdp_primitive_state_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_rdp_primitive_state_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_rdp_primitive_state_test COMMAND n64_rdp_primitive_state_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_primitive_depth/rdp_primitive_depth.z64)
set_tests_properties(n64_rdp_primitive_state_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 60)
add_test(NAME n64_rdp_primitive_depth_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_rdp_primitive_depth_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_rdp_primitive_depth)
set_tests_properties(n64_rdp_primitive_depth_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 240)
add_test(NAME n64_rdp_primitive_depth COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_primitive_depth/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_primitive_depth)
set_tests_properties(n64_rdp_primitive_depth PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_primitive_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_primitive_depth/rdp_primitive_depth.z64 primitive)
set_tests_properties(n64_rdp_primitive_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)

add_executable(n64_rdp_dither_test cpp/n64_rdp_dither_test.cpp)
target_include_directories(n64_rdp_dither_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_dither_test COMMAND n64_rdp_dither_test)
set_tests_properties(n64_rdp_dither_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 180)
add_test(NAME n64_rdp_dither_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_rdp_dither_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_rdp_dither)
set_tests_properties(n64_rdp_dither_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 240)
add_test(NAME n64_rdp_ordered_dither COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_ordered_dither/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_ordered_dither)
set_tests_properties(n64_rdp_ordered_dither PROPERTIES LABELS "accuracy;owned" TIMEOUT 700)
add_test(NAME n64_rdp_dither_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_ordered_dither/rdp_ordered_dither.z64 hidden)
set_tests_properties(n64_rdp_dither_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)

add_executable(n64_rdp_copy_test cpp/n64_rdp_copy_test.cpp)
target_include_directories(n64_rdp_copy_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_copy_test COMMAND n64_rdp_copy_test)
set_tests_properties(n64_rdp_copy_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 90)
add_test(NAME n64_rdp_copy_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_rdp_copy_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_rdp_copy)
set_tests_properties(n64_rdp_copy_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 240)
add_test(NAME n64_rdp_copy_rectangles COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_copy_rectangles/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_copy_rectangles)
set_tests_properties(n64_rdp_copy_rectangles PROPERTIES LABELS "accuracy;owned" TIMEOUT 1400)
add_test(NAME n64_rdp_copy_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_copy_rectangles/rdp_copy_rectangles.z64 hidden)
set_tests_properties(n64_rdp_copy_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)

add_executable(n64_pif_joybus_test cpp/n64_pif_joybus_test.cpp)
target_include_directories(n64_pif_joybus_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_pif_joybus_test COMMAND n64_pif_joybus_test)
set_tests_properties(n64_pif_joybus_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 90)
add_test(NAME n64_controller_cached_reads COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_cached_reads/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_controller_cached_reads)
set_tests_properties(n64_controller_cached_reads PROPERTIES LABELS "accuracy;owned" TIMEOUT 300)
add_executable(n64_pif_state_test cpp/n64_pif_state_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/n64/src/machine.cpp ${CMAKE_SOURCE_DIR}/cores/n64/src/vr4300.cpp)
target_include_directories(n64_pif_state_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src ${CMAKE_SOURCE_DIR}/include)
add_test(NAME n64_pif_state_test COMMAND n64_pif_state_test
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_cached_reads/controller_cached_reads.z64)
set_tests_properties(n64_pif_state_test PROPERTIES LABELS "unit;accuracy;owned" TIMEOUT 60)
add_test(NAME n64_pif_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/controller_cached_reads/controller_cached_reads.z64 joybus)
set_tests_properties(n64_pif_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)
add_test(NAME n64_pif_joybus_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_pif_joybus_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_pif_joybus)
set_tests_properties(n64_pif_joybus_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 240)

add_executable(n64_rdp_rectangle_test cpp/n64_rdp_rectangle_test.cpp)
target_include_directories(n64_rdp_rectangle_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/n64/src)
add_test(NAME n64_rdp_rectangle_test COMMAND n64_rdp_rectangle_test)
set_tests_properties(n64_rdp_rectangle_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 90)
add_test(NAME n64_rdp_rectangle_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/n64_rdp_rectangle_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/n64_rdp_rectangle)
set_tests_properties(n64_rdp_rectangle_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 240)
add_test(NAME n64_rdp_texture_rectangles COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_texture_rectangles/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/n64_rdp_texture_rectangles)
set_tests_properties(n64_rdp_texture_rectangles PROPERTIES LABELS "accuracy;owned" TIMEOUT 1400)
add_test(NAME n64_rdp_rectangle_checkpoint_test COMMAND n64_rdp_packet_plugin_test
    $<TARGET_FILE:n64_plugin> ${CMAKE_SOURCE_DIR}/tests/roms-src/n64/rdp_texture_rectangles/rdp_texture_rectangles.z64 hidden)
set_tests_properties(n64_rdp_rectangle_checkpoint_test PROPERTIES LABELS "integration;netplay;accuracy;owned" TIMEOUT 60)

add_executable(nes_test_channel_plugin_test cpp/nes_test_channel_plugin_test.cpp)
target_include_directories(nes_test_channel_plugin_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(nes_test_channel_plugin_test PRIVATE ${SDL2_LIBRARIES})
add_dependencies(nes_test_channel_plugin_test nes_plugin)
add_test(NAME nes_test_channel_plugin_test COMMAND nes_test_channel_plugin_test $<TARGET_FILE:nes_plugin> ${CMAKE_BINARY_DIR}/artifacts/owned/nes_result_channel)
set_tests_properties(nes_test_channel_plugin_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 30)

add_executable(gb_test_channel_plugin_test cpp/gb_test_channel_plugin_test.cpp)
target_include_directories(gb_test_channel_plugin_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(gb_test_channel_plugin_test PRIVATE ${SDL2_LIBRARIES})
add_dependencies(gb_test_channel_plugin_test gb_plugin)
add_test(NAME gb_test_channel_plugin_test COMMAND gb_test_channel_plugin_test $<TARGET_FILE:gb_plugin> ${CMAKE_BINARY_DIR}/artifacts/owned/gb_result_channel)
set_tests_properties(gb_test_channel_plugin_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 30)

add_executable(gb_unbanked_ram_test cpp/gb_unbanked_ram_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/cartridge.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc1.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc3.cpp
    ${CMAKE_SOURCE_DIR}/cores/gb/src/mbc/mbc5.cpp)
target_include_directories(gb_unbanked_ram_test PRIVATE ${CMAKE_SOURCE_DIR}/cores/gb/src)
add_test(NAME gb_unbanked_ram_test COMMAND gb_unbanked_ram_test)
set_tests_properties(gb_unbanked_ram_test PROPERTIES LABELS "unit;accuracy" TIMEOUT 30)

add_test(NAME gb_unbanked_ram_mutants COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/gb_unbanked_ram_mutants.py --compiler ${CMAKE_CXX_COMPILER}
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/component/gb_unbanked_ram)
set_tests_properties(gb_unbanked_ram_mutants PROPERTIES LABELS "unit;accuracy" TIMEOUT 120)

add_executable(gba_test_channel_plugin_test cpp/gba_test_channel_plugin_test.cpp)
target_include_directories(gba_test_channel_plugin_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(gba_test_channel_plugin_test PRIVATE ${SDL2_LIBRARIES})
add_dependencies(gba_test_channel_plugin_test gba_plugin)
add_test(NAME gba_test_channel_plugin_test COMMAND gba_test_channel_plugin_test $<TARGET_FILE:gba_plugin> ${CMAKE_BINARY_DIR}/artifacts/owned/gba_result_channel)
set_tests_properties(gba_test_channel_plugin_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 30)

add_executable(snes_test_channel_test cpp/snes_test_channel_test.cpp
    ${CMAKE_SOURCE_DIR}/cores/snes/src/test_channel.cpp)
target_include_directories(snes_test_channel_test PRIVATE ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/cores/snes/src)
add_test(NAME snes_test_channel COMMAND snes_test_channel_test)
set_tests_properties(snes_test_channel PROPERTIES LABELS "unit;fast")

add_executable(snes_test_channel_plugin_test cpp/snes_test_channel_plugin_test.cpp)
target_include_directories(snes_test_channel_plugin_test PRIVATE ${CMAKE_SOURCE_DIR}/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(snes_test_channel_plugin_test PRIVATE ${SDL2_LIBRARIES})
add_dependencies(snes_test_channel_plugin_test snes_plugin)
add_test(NAME snes_test_channel_plugin_test COMMAND snes_test_channel_plugin_test $<TARGET_FILE:snes_plugin>
    ${CMAKE_BINARY_DIR}/artifacts/owned/snes_result_channel)
set_tests_properties(snes_test_channel_plugin_test PROPERTIES LABELS "integration;accuracy;owned" TIMEOUT 30)

add_test(NAME cli_exit_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cli_exit_test.py $<TARGET_FILE:veloce>)
set_tests_properties(cli_exit_test PROPERTIES LABELS "integration;fast" TIMEOUT 45)

add_test(NAME nes_frame_irq COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/roms-src/nes/frame_irq/run.py
    --emulator $<TARGET_FILE:veloce> --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/nes_frame_irq)
set_tests_properties(nes_frame_irq PROPERTIES LABELS "accuracy;owned" TIMEOUT 180)

add_test(NAME snes_stack_boundaries COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/snes/stack_boundaries/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/snes_stack_boundaries)
set_tests_properties(snes_stack_boundaries PROPERTIES LABELS "accuracy;owned" TIMEOUT 400)

add_test(NAME snes_decimal_sbc COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/tests/roms-src/snes/decimal_sbc/run.py --emulator $<TARGET_FILE:veloce>
    --artifacts ${CMAKE_BINARY_DIR}/artifacts/owned/snes_decimal_sbc)
set_tests_properties(snes_decimal_sbc PROPERTIES LABELS "accuracy;owned" TIMEOUT 400)

add_executable(audio_pipeline_test cpp/audio_pipeline_test.cpp)
target_include_directories(audio_pipeline_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(audio_pipeline_test PRIVATE audio_default)
add_test(NAME audio_pipeline_test COMMAND audio_pipeline_test)
set_tests_properties(audio_pipeline_test PROPERTIES LABELS "integration;fast")

add_library(audio_frontend_fixture SHARED cpp/audio_frontend_fixture.cpp)
add_library(audio_core_fixture SHARED cpp/audio_core_fixture.cpp)
foreach(fixture audio_frontend_fixture audio_core_fixture)
    target_include_directories(${fixture} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    set_target_properties(${fixture} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fixtures/audio)
endforeach()
find_program(VELOCE_XVFB_RUN xvfb-run)
if(VELOCE_XVFB_RUN)
    add_test(NAME audio_frontend_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/audio_frontend_test.py
        --emulator $<TARGET_FILE:veloce> --audio $<TARGET_FILE:audio_frontend_fixture>
        --core $<TARGET_FILE:audio_core_fixture> --artifacts ${CMAKE_BINARY_DIR}/artifacts/audio_frontend
        --xvfb ${VELOCE_XVFB_RUN})
    set_tests_properties(audio_frontend_test PROPERTIES LABELS "integration" TIMEOUT 100)
endif()

add_executable(audio_lifecycle_test cpp/audio_lifecycle_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_manager.cpp ${CMAKE_SOURCE_DIR}/src/core/plugin_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_config.cpp ${CMAKE_SOURCE_DIR}/src/core/paths_config.cpp)
target_include_directories(audio_lifecycle_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(audio_lifecycle_test PRIVATE nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})
add_dependencies(audio_lifecycle_test audio_frontend_fixture)
add_test(NAME audio_lifecycle_test COMMAND audio_lifecycle_test $<TARGET_FILE:audio_frontend_fixture>
    ${CMAKE_BINARY_DIR}/artifacts/audio_lifecycle)
set_tests_properties(audio_lifecycle_test PROPERTIES LABELS "integration;fast" TIMEOUT 30)

add_executable(input_plugin_test cpp/input_plugin_test.cpp)
target_include_directories(input_plugin_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(input_plugin_test PRIVATE input_default)
add_test(NAME input_plugin_test COMMAND input_plugin_test)
set_tests_properties(input_plugin_test PROPERTIES LABELS "integration;fast")
add_library(input_frontend_fixture SHARED cpp/input_frontend_fixture.cpp)
add_library(input_core_fixture SHARED cpp/input_core_fixture.cpp)
foreach(fixture input_frontend_fixture input_core_fixture)
    target_include_directories(${fixture} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    set_target_properties(${fixture} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/fixtures/input)
endforeach()
if(VELOCE_XVFB_RUN)
    add_test(NAME input_frontend_test COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/input_frontend_test.py
        --emulator $<TARGET_FILE:veloce> --input $<TARGET_FILE:input_frontend_fixture>
        --core $<TARGET_FILE:input_core_fixture> --artifacts ${CMAKE_BINARY_DIR}/artifacts/input_frontend
        --xvfb ${VELOCE_XVFB_RUN})
    set_tests_properties(input_frontend_test PROPERTIES LABELS "integration" TIMEOUT 60)
endif()

add_executable(input_lifecycle_test cpp/input_lifecycle_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_manager.cpp ${CMAKE_SOURCE_DIR}/src/core/plugin_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/core/plugin_config.cpp ${CMAKE_SOURCE_DIR}/src/core/paths_config.cpp)
target_include_directories(input_lifecycle_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(input_lifecycle_test PRIVATE nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})
add_dependencies(input_lifecycle_test input_frontend_fixture)
add_test(NAME input_lifecycle_test COMMAND input_lifecycle_test $<TARGET_FILE:input_frontend_fixture>
    ${CMAKE_BINARY_DIR}/artifacts/input_lifecycle)
set_tests_properties(input_lifecycle_test PROPERTIES LABELS "integration;fast" TIMEOUT 30)
