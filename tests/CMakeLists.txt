# Veloce test + accuracy framework - CTest integration.
#
# Included from the root CMakeLists via add_subdirectory(tests) when
# BUILD_TESTING is on (the default when include(CTest) is active).
#
# Targets registered:
#   testkit_selftest    - fast pure-logic unit test (no ROMs, no emulator)
#   test_file_sink      - C++ unit test of the VELOCE_TEST_OUT sink (stub plugin)
#   validate_configs    - schema validation of every test_config.json
#   accuracy_<console>  - full per-console accuracy run (needs network for ROMs)
#   accuracy_all        - cross-console aggregate scorecard
#
# Run:  ctest --test-dir build                 # fast gates (selftest+validate)
#       ctest --test-dir build -L accuracy      # full accuracy suites (slow)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

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
