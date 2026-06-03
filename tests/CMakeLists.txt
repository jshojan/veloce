# Veloce test + accuracy framework - CTest integration.
#
# Included from the root CMakeLists via add_subdirectory(tests) when
# BUILD_TESTING is on (the default when include(CTest) is active).
#
# Targets registered:
#   testkit_selftest    - fast pure-logic unit test (no ROMs, no emulator)
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

# --- per-console accuracy suites (slow: download ROMs, run emulator) ------
foreach(console nes snes gb gba)
    add_test(
        NAME accuracy_${console}
        COMMAND ${Python3_EXECUTABLE}
                ${VELOCE_TESTS_DIR}/../cores/${console}/tests/test_runner.py --json
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
