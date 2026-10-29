cmake_minimum_required(VERSION 3.22)
project(CS80Reference VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CS80_BUILD_PLUGIN "Build the JUCE plugin and standalone application" ON)
option(CS80_BUILD_TESTS "Build core tests" ON)

add_library(cs80_core STATIC
    src/core/Engine.cpp
    src/core/FactoryPresets.cpp
)
target_include_directories(cs80_core PUBLIC src)
if (MSVC)
    target_compile_options(cs80_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(cs80_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (CS80_BUILD_TESTS)
    add_executable(cs80_tests tests/CoreTests.cpp)
    target_link_libraries(cs80_tests PRIVATE cs80_core)
    if (MSVC)
        target_compile_options(cs80_tests PRIVATE /W4)
    else()
        target_compile_options(cs80_tests PRIVATE -Wall -Wextra)
    endif()
    enable_testing()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME cs80_parity_harness_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parity_harness.py)
    set_tests_properties(cs80_parity_harness_tests PROPERTIES TIMEOUT 60)
    add_executable(cs80_parity_mode_tests tests/ParityModeTests.cpp)
    target_link_libraries(cs80_parity_mode_tests PRIVATE cs80_core)
    add_test(NAME cs80_parity_mode_tests COMMAND cs80_parity_mode_tests)
    set_tests_properties(cs80_parity_mode_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_live_envelope_timing_tests tests/LiveEnvelopeTimingTests.cpp)
    target_link_libraries(cs80_live_envelope_timing_tests PRIVATE cs80_core)
    add_test(NAME cs80_live_envelope_timing_tests COMMAND cs80_live_envelope_timing_tests)
    set_tests_properties(cs80_live_envelope_timing_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_active_tail_continuity_tests tests/ActiveTailContinuityTests.cpp)
    target_link_libraries(cs80_active_tail_continuity_tests PRIVATE cs80_core)
    add_test(NAME cs80_active_tail_continuity_tests COMMAND cs80_active_tail_continuity_tests)
    set_tests_properties(cs80_active_tail_continuity_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_core_tests COMMAND cs80_tests)
    add_executable(cs80_hardware_control_tests tests/HardwareControlTests.cpp)
    target_include_directories(cs80_hardware_control_tests PRIVATE src)
    add_test(NAME cs80_hardware_control_tests COMMAND cs80_hardware_control_tests)
    add_executable(cs80_ig00156_reference_tests tests/IG00156ReferenceTests.cpp)
    target_include_directories(cs80_ig00156_reference_tests PRIVATE src)
    if (MSVC)
        target_compile_options(cs80_ig00156_reference_tests PRIVATE /W4)
    else()
        target_compile_options(cs80_ig00156_reference_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cs80_ig00156_reference_tests COMMAND cs80_ig00156_reference_tests)
    set_tests_properties(cs80_ig00156_reference_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_keyboard_contour_reference_tests tests/KeyboardContourReferenceTests.cpp)
    target_include_directories(cs80_keyboard_contour_reference_tests PRIVATE src)
    if (MSVC)
        target_compile_options(cs80_keyboard_contour_reference_tests PRIVATE /W4)
    else()
        target_compile_options(cs80_keyboard_contour_reference_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cs80_keyboard_contour_reference_tests COMMAND cs80_keyboard_contour_reference_tests)
    set_tests_properties(cs80_keyboard_contour_reference_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_loaded_control_reference_tests tests/LoadedControlReferenceTests.cpp)
    target_include_directories(cs80_loaded_control_reference_tests PRIVATE src)
    add_test(NAME cs80_loaded_control_reference_tests COMMAND cs80_loaded_control_reference_tests)
    set_tests_properties(cs80_loaded_control_reference_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_loaded_control_rank_tests tests/LoadedControlRankTests.cpp)
    target_include_directories(cs80_loaded_control_rank_tests PRIVATE src)
    add_test(NAME cs80_loaded_control_rank_tests COMMAND cs80_loaded_control_rank_tests)
    set_tests_properties(cs80_loaded_control_rank_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_loaded_control_transient_tests tests/LoadedControlTransientTests.cpp)
    target_include_directories(cs80_loaded_control_transient_tests PRIVATE src)
    add_test(NAME cs80_loaded_control_transient_tests COMMAND cs80_loaded_control_transient_tests)
    set_tests_properties(cs80_loaded_control_transient_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_oe2_filters_tests tests/Oe2FiltersTests.cpp)
    target_include_directories(cs80_oe2_filters_tests PRIVATE src)
    add_test(NAME cs80_oe2_filters_tests COMMAND cs80_oe2_filters_tests)
    set_tests_properties(cs80_oe2_filters_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_pra_wah_tests tests/PraWahTests.cpp)
    target_include_directories(cs80_pra_wah_tests PRIVATE src)
    add_test(NAME cs80_pra_wah_tests COMMAND cs80_pra_wah_tests)
    set_tests_properties(cs80_pra_wah_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_mn3001_packet_tests tests/Mn3001PacketTests.cpp)
    target_include_directories(cs80_mn3001_packet_tests PRIVATE src)
    add_test(NAME cs80_mn3001_packet_tests COMMAND cs80_mn3001_packet_tests)
    set_tests_properties(cs80_mn3001_packet_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_performance_control_tests tests/PerformanceControlTests.cpp)
    target_link_libraries(cs80_performance_control_tests PRIVATE cs80_core)
    if (MSVC)
        target_compile_options(cs80_performance_control_tests PRIVATE /W4)
    else()
        target_compile_options(cs80_performance_control_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cs80_performance_control_tests COMMAND cs80_performance_control_tests)
    set_tests_properties(cs80_performance_control_tests PROPERTIES TIMEOUT 120)
    if (UNIX)
        add_test(NAME cs80_validator_retry_tests
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-pluginval-retry.sh
                              ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-pluginval-linux.sh)
        set_tests_properties(cs80_validator_retry_tests PROPERTIES TIMEOUT 30)
    endif()
endif()

add_executable(cs80_render src/tools/render_cli.cpp)
target_link_libraries(cs80_render PRIVATE cs80_core)

add_executable(cs80_dsp_probe src/tools/dsp_probe.cpp)
target_link_libraries(cs80_dsp_probe PRIVATE cs80_core)
add_executable(cs80_parity_capture src/tools/parity_capture.cpp)
target_link_libraries(cs80_parity_capture PRIVATE cs80_core)
if (CS80_BUILD_TESTS)
    add_test(NAME cs80_parity_capture_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parity_capture.py
                     --capture $<TARGET_FILE:cs80_parity_capture>)
    set_tests_properties(cs80_parity_capture_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_dsp_probe_tests COMMAND cs80_dsp_probe ${CMAKE_CURRENT_BINARY_DIR}/dsp-probe)
    set_tests_properties(cs80_dsp_probe_tests PROPERTIES TIMEOUT 120)
endif()

# ---------------------------------------------------------------------------
# JUCE plugin / standalone
# ---------------------------------------------------------------------------
if (CS80_BUILD_PLUGIN)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/juce/CMakeLists.txt")
        add_subdirectory(deps/juce)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                                  GIT_TAG 8.0.6)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(CS80_PLUGIN_FORMATS VST3 Standalone)
    if (APPLE)
        list(APPEND CS80_PLUGIN_FORMATS AU)
    endif()

    juce_add_plugin(CS80Reference
        COMPANY_NAME "Reference Instruments"
        BUNDLE_ID "com.referenceinstruments.cs80"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE RefI
        PLUGIN_CODE Cs80
        FORMATS ${CS80_PLUGIN_FORMATS}
        PRODUCT_NAME "CS-80 Reference"
    )

    target_sources(CS80Reference PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/UiComponents.cpp
        src/plugin/AdvancedPanel.cpp
        src/plugin/PresetBrowser.cpp
    )
    target_include_directories(CS80Reference PRIVATE src)
    target_link_libraries(CS80Reference PRIVATE
        cs80_core
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    )
    target_compile_definitions(CS80Reference PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )

    # Headless UI snapshot tool, built from the same editor/processor sources.
    juce_add_console_app(CS80Snapshots PRODUCT_NAME "CS80Snapshots")
    target_sources(CS80Snapshots PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/UiComponents.cpp
        src/plugin/AdvancedPanel.cpp
        src/plugin/PresetBrowser.cpp
        src/tools/ui_snapshot.cpp
    )
    target_include_directories(CS80Snapshots PRIVATE src)
    target_link_libraries(CS80Snapshots PRIVATE
        cs80_core
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    )
    target_compile_definitions(CS80Snapshots PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )

    # The snapshot binary doubles as a headless plugin/UI smoke test. It
    # exercises the APVTS, preset and state round-trips, MIDI rendering, the
    # editor at two sizes, all advanced pages, and user preset storage. The
    # console tool uses JUCE's software renderer, so it is registered on all
    # three desktop platforms rather than only Unix.
    add_test(NAME cs80_ui_smoke
             COMMAND CS80Snapshots --out ${CMAKE_CURRENT_BINARY_DIR}/ui-smoke --self-test)
    set_tests_properties(cs80_ui_smoke PROPERTIES TIMEOUT 120)
endif()
