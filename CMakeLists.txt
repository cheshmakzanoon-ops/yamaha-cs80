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
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME cs80_ui_environment_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ui_smoke_environment.py)
        set_tests_properties(cs80_ui_environment_tests PROPERTIES TIMEOUT 45)
    endif()
    find_package(Threads REQUIRED)
    add_executable(cs80_continuous_voice_circuit_tests tests/ContinuousVoiceCircuitTests.cpp)
    target_include_directories(cs80_continuous_voice_circuit_tests PRIVATE src)
    add_test(NAME cs80_continuous_voice_circuit_tests COMMAND cs80_continuous_voice_circuit_tests)
    set_tests_properties(cs80_continuous_voice_circuit_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_continuous_voice_circuit_audit_tests tests/ContinuousVoiceCircuitAuditTests.cpp)
    target_include_directories(cs80_continuous_voice_circuit_audit_tests PRIVATE src)
    add_test(NAME cs80_continuous_voice_circuit_audit_tests COMMAND cs80_continuous_voice_circuit_audit_tests)
    set_tests_properties(cs80_continuous_voice_circuit_audit_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_output_meter_display_tests tests/OutputMeterDisplayTests.cpp)
    target_include_directories(cs80_output_meter_display_tests PRIVATE src)
    add_test(NAME cs80_output_meter_display_tests COMMAND cs80_output_meter_display_tests)
    set_tests_properties(cs80_output_meter_display_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_output_meter_tests tests/OutputMeterTests.cpp)
    target_include_directories(cs80_output_meter_tests PRIVATE src)
    add_test(NAME cs80_output_meter_tests COMMAND cs80_output_meter_tests)
    set_tests_properties(cs80_output_meter_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_output_meter_audit_tests tests/OutputMeterAuditTests.cpp)
    target_include_directories(cs80_output_meter_audit_tests PRIVATE src)
    target_link_libraries(cs80_output_meter_audit_tests PRIVATE Threads::Threads)
    add_test(NAME cs80_output_meter_audit_tests COMMAND cs80_output_meter_audit_tests)
    set_tests_properties(cs80_output_meter_audit_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_keyboard_pressure_audit_tests tests/KeyboardPressureAuditTests.cpp)
    target_include_directories(cs80_keyboard_pressure_audit_tests PRIVATE src)
    target_link_libraries(cs80_keyboard_pressure_audit_tests PRIVATE Threads::Threads)
    add_test(NAME cs80_keyboard_pressure_audit_tests COMMAND cs80_keyboard_pressure_audit_tests)
    set_tests_properties(cs80_keyboard_pressure_audit_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_keyboard_event_queue_tests tests/KeyboardEventQueueTests.cpp)
    target_include_directories(cs80_keyboard_event_queue_tests PRIVATE src)
    target_link_libraries(cs80_keyboard_event_queue_tests PRIVATE Threads::Threads)
    add_test(NAME cs80_keyboard_event_queue_tests COMMAND cs80_keyboard_event_queue_tests)
    set_tests_properties(cs80_keyboard_event_queue_tests PROPERTIES TIMEOUT 120)
    if (NOT MSVC)
        add_test(NAME cs80_keyboard_epoch_interleaving_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_keyboard_epoch_interleaving.py
                         --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(cs80_keyboard_epoch_interleaving_tests PROPERTIES TIMEOUT 180)
    endif()
    add_executable(cs80_parity_mode_tests tests/ParityModeTests.cpp)
    target_link_libraries(cs80_parity_mode_tests PRIVATE cs80_core)
    add_test(NAME cs80_parity_mode_tests COMMAND cs80_parity_mode_tests)
    set_tests_properties(cs80_parity_mode_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_source_performance_tests tests/SourcePerformanceTests.cpp)
    target_link_libraries(cs80_source_performance_tests PRIVATE cs80_core)
    add_test(NAME cs80_source_performance_tests COMMAND cs80_source_performance_tests)
    set_tests_properties(cs80_source_performance_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_source_performance_audit_tests tests/SourcePerformanceAuditTests.cpp)
    target_link_libraries(cs80_source_performance_audit_tests PRIVATE cs80_core)
    add_test(NAME cs80_source_performance_audit_tests COMMAND cs80_source_performance_audit_tests)
    set_tests_properties(cs80_source_performance_audit_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_envelope_precision_tests tests/EnvelopePrecisionTests.cpp)
    target_link_libraries(cs80_envelope_precision_tests PRIVATE cs80_core)
    add_test(NAME cs80_envelope_precision_tests COMMAND cs80_envelope_precision_tests)
    set_tests_properties(cs80_envelope_precision_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_ig00156_filter_tests tests/Ig00156FilterTests.cpp)
    target_link_libraries(cs80_ig00156_filter_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00156_filter_tests COMMAND cs80_ig00156_filter_tests)
    set_tests_properties(cs80_ig00156_filter_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_envelope_rc_model_tests tests/EnvelopeRcModelTests.cpp)
    target_link_libraries(cs80_envelope_rc_model_tests PRIVATE cs80_core)
    add_test(NAME cs80_envelope_rc_model_tests COMMAND cs80_envelope_rc_model_tests)
    set_tests_properties(cs80_envelope_rc_model_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_live_envelope_level_tests tests/LiveEnvelopeLevelTests.cpp)
    target_link_libraries(cs80_live_envelope_level_tests PRIVATE cs80_core)
    add_test(NAME cs80_live_envelope_level_tests COMMAND cs80_live_envelope_level_tests)
    set_tests_properties(cs80_live_envelope_level_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_live_envelope_timing_tests tests/LiveEnvelopeTimingTests.cpp)
    target_link_libraries(cs80_live_envelope_timing_tests PRIVATE cs80_core)
    add_test(NAME cs80_live_envelope_timing_tests COMMAND cs80_live_envelope_timing_tests)
    set_tests_properties(cs80_live_envelope_timing_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_active_tail_continuity_tests tests/ActiveTailContinuityTests.cpp)
    target_link_libraries(cs80_active_tail_continuity_tests PRIVATE cs80_core)
    add_test(NAME cs80_active_tail_continuity_tests COMMAND cs80_active_tail_continuity_tests)
    set_tests_properties(cs80_active_tail_continuity_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_core_tests COMMAND cs80_tests)
    # Experimental linear-path model only; nonlinear Engine/CPU gates are not qualified.
    add_executable(cs80_experimental_oscillator_tests tests/ReconstructedOscillatorTests.cpp)
    target_include_directories(cs80_experimental_oscillator_tests PRIVATE src)
    add_test(NAME cs80_experimental_oscillator_tests COMMAND cs80_experimental_oscillator_tests no-benchmark)
    set_tests_properties(cs80_experimental_oscillator_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_source_factory_integration_tests tests/SourceFactoryIntegrationTests.cpp)
    target_link_libraries(cs80_source_factory_integration_tests PRIVATE cs80_core)
    add_test(NAME cs80_source_factory_integration_tests COMMAND cs80_source_factory_integration_tests)
    set_tests_properties(cs80_source_factory_integration_tests PROPERTIES TIMEOUT 600)
    add_executable(cs80_source_factory_tone_tests tests/SourceFactoryToneTests.cpp)
    target_include_directories(cs80_source_factory_tone_tests PRIVATE src)
    add_test(NAME cs80_source_factory_tone_tests COMMAND cs80_source_factory_tone_tests)
    set_tests_properties(cs80_source_factory_tone_tests PROPERTIES TIMEOUT 60)
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
    add_executable(cs80_output_tap_integration_tests tests/OutputTapIntegrationTests.cpp)
    target_link_libraries(cs80_output_tap_integration_tests PRIVATE cs80_core)
    add_test(NAME cs80_output_tap_integration_tests COMMAND cs80_output_tap_integration_tests)
    set_tests_properties(cs80_output_tap_integration_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_output_tap_circuit_tests tests/OutputTapCircuitTests.cpp)
    target_include_directories(cs80_output_tap_circuit_tests PRIVATE src)
    add_test(NAME cs80_output_tap_circuit_tests COMMAND cs80_output_tap_circuit_tests)
    set_tests_properties(cs80_output_tap_circuit_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_output_taps_tests tests/OutputTapsTests.cpp)
    target_include_directories(cs80_output_taps_tests PRIVATE src)
    add_test(NAME cs80_output_taps_tests COMMAND cs80_output_taps_tests)
    set_tests_properties(cs80_output_taps_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_level_control_tests tests/LevelControlTests.cpp)
    target_include_directories(cs80_level_control_tests PRIVATE src)
    add_test(NAME cs80_level_control_tests COMMAND cs80_level_control_tests)
    set_tests_properties(cs80_level_control_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_resonance_control_tests tests/ResonanceControlTests.cpp)
    target_include_directories(cs80_resonance_control_tests PRIVATE src)
    add_test(NAME cs80_resonance_control_tests COMMAND cs80_resonance_control_tests)
    set_tests_properties(cs80_resonance_control_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_oe2_filters_tests tests/Oe2FiltersTests.cpp)
    target_include_directories(cs80_oe2_filters_tests PRIVATE src)
    add_test(NAME cs80_oe2_filters_tests COMMAND cs80_oe2_filters_tests)
    set_tests_properties(cs80_oe2_filters_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_pra_wah_integration_tests tests/PraWahIntegrationTests.cpp)
    target_link_libraries(cs80_pra_wah_integration_tests PRIVATE cs80_core)
    add_test(NAME cs80_pra_wah_integration_tests COMMAND cs80_pra_wah_integration_tests)
    set_tests_properties(cs80_pra_wah_integration_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_pra_wah_circuit_tests tests/PraWahCircuitTests.cpp)
    target_include_directories(cs80_pra_wah_circuit_tests PRIVATE src)
    add_test(NAME cs80_pra_wah_circuit_tests COMMAND cs80_pra_wah_circuit_tests)
    set_tests_properties(cs80_pra_wah_circuit_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_pra_wah_tests tests/PraWahTests.cpp)
    target_include_directories(cs80_pra_wah_tests PRIVATE src)
    add_test(NAME cs80_pra_wah_tests COMMAND cs80_pra_wah_tests)
    set_tests_properties(cs80_pra_wah_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_bbd_chorus_tests tests/BbdChorusTests.cpp)
    target_include_directories(cs80_bbd_chorus_tests PRIVATE src)
    add_test(NAME cs80_bbd_chorus_tests COMMAND cs80_bbd_chorus_tests)
    set_tests_properties(cs80_bbd_chorus_tests PROPERTIES TIMEOUT 60)
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

    # [ws1-voice] register this workstream's core tests below this line.

    # [ws2-engine] register this workstream's core tests below this line.

    # [ws3-keyboard] register this workstream's core tests below this line.

    # [ws4-circuits] register this workstream's core tests below this line.
    add_executable(cs80_ig00150_sub_tests tests/Ig00150SubTests.cpp)
    target_link_libraries(cs80_ig00150_sub_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00150_sub_tests COMMAND cs80_ig00150_sub_tests)
    set_tests_properties(cs80_ig00150_sub_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_ring_modulator_circuit_tests tests/RingModulatorCircuitTests.cpp)
    target_link_libraries(cs80_ring_modulator_circuit_tests PRIVATE cs80_core)
    add_test(NAME cs80_ring_modulator_circuit_tests COMMAND cs80_ring_modulator_circuit_tests)
    set_tests_properties(cs80_ring_modulator_circuit_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_oe2_stereo_chorus_tests tests/Oe2StereoChorusTests.cpp)
    target_include_directories(cs80_oe2_stereo_chorus_tests PRIVATE src tests)
    add_test(NAME cs80_oe2_stereo_chorus_tests COMMAND cs80_oe2_stereo_chorus_tests)
    set_tests_properties(cs80_oe2_stereo_chorus_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_pra_optical_cell_tests tests/PraOpticalCellTests.cpp)
    target_link_libraries(cs80_pra_optical_cell_tests PRIVATE cs80_core)
    add_test(NAME cs80_pra_optical_cell_tests COMMAND cs80_pra_optical_cell_tests)
    set_tests_properties(cs80_pra_optical_cell_tests PROPERTIES TIMEOUT 120)

    # [ws5-modern] register this workstream's core tests below this line.

    # [ws6-library] register this workstream's core tests below this line.

    # [ws7-panel] register this workstream's core tests below this line.

    # [ws8-studio] register this workstream's core tests below this line.
endif()

add_executable(cs80_render src/tools/render_cli.cpp)
target_link_libraries(cs80_render PRIVATE cs80_core)

add_executable(cs80_source_factory_audition src/tools/source_factory_audition.cpp)
target_link_libraries(cs80_source_factory_audition PRIVATE cs80_core)

add_executable(cs80_voice_continuity_probe src/tools/voice_continuity_probe.cpp)
target_include_directories(cs80_voice_continuity_probe PRIVATE src)

add_executable(cs80_source_factory_benchmark src/tools/source_factory_benchmark.cpp)
target_link_libraries(cs80_source_factory_benchmark PRIVATE cs80_core)

add_executable(cs80_dsp_probe src/tools/dsp_probe.cpp)
target_link_libraries(cs80_dsp_probe PRIVATE cs80_core)
add_executable(cs80_engine_audit src/tools/engine_audit.cpp)
target_link_libraries(cs80_engine_audit PRIVATE cs80_core)
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
                                  # Official 8.0.11: fixes Linux locale-query restoration.
                                  GIT_TAG c352e2489000ffeb9e397051377d75eef2efe641)
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

    # Editor and processor sources shared by the plugin and the snapshot tool.
    set(CS80_PLUGIN_SOURCES
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/UiComponents.cpp
        src/plugin/AdvancedPanel.cpp
        src/plugin/PresetBrowser.cpp

        # [ws1-voice] add this workstream's plugin sources below this line.

        # [ws2-engine] add this workstream's plugin sources below this line.

        # [ws3-keyboard] add this workstream's plugin sources below this line.

        # [ws4-circuits] add this workstream's plugin sources below this line.

        # [ws5-modern] add this workstream's plugin sources below this line.

        # [ws6-library] add this workstream's plugin sources below this line.

        # [ws7-panel] add this workstream's plugin sources below this line.
        src/plugin/ArtCommon.cpp
        src/plugin/PanelWidgets.cpp
        src/plugin/ChannelRow.cpp
        src/plugin/CentreRow.cpp
        src/plugin/PerformanceDeck.cpp
        src/plugin/MemoryLid.cpp
        src/plugin/EditorPreferences.cpp

        # [ws8-studio] add this workstream's plugin sources below this line.

    )
    target_sources(CS80Reference PRIVATE ${CS80_PLUGIN_SOURCES})
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
        ${CS80_PLUGIN_SOURCES}
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

    # [ws7-panel] The panel typeface (Barlow Condensed, SIL OFL 1.1), embedded so the panel draws the same on
    # every OS. Provenance: assets/fonts/barlow-condensed/SOURCE.txt.
    juce_add_binary_data(cs80_panel_fonts
        HEADER_NAME PanelFontData.h
        NAMESPACE cs80_panel_fonts
        SOURCES
            assets/fonts/barlow-condensed/BarlowCondensed-Medium.ttf
            assets/fonts/barlow-condensed/BarlowCondensed-SemiBold.ttf)
    target_link_libraries(CS80Reference PRIVATE cs80_panel_fonts)
    target_link_libraries(CS80Snapshots PRIVATE cs80_panel_fonts)
    # [ws7-panel] Golden images of the main view (tests/PanelEditorTests.h; refresh with CS80_REFRESH_GOLDENS=1).
    target_compile_definitions(CS80Snapshots PRIVATE CS80_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden")

    # This offline integration harness keeps many processor fixtures alive on
    # the stack (including nested host tests). Match the ordinary 8 MiB Unix
    # test-process budget instead of MSVC's 1 MiB executable default. This is
    # deliberately NOT applied to the shipping plugin or Standalone targets.
    if (MSVC)
        target_link_options(CS80Snapshots PRIVATE /STACK:8388608)
    endif()

    # The snapshot binary doubles as a headless plugin/UI smoke test. It
    # exercises the APVTS, preset and state round-trips, MIDI rendering, the
    # editor at two sizes, all advanced pages, and user preset storage. The
    # console tool uses JUCE's software renderer, so it is registered on all
    # three desktop platforms rather than only Unix.
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME cs80_ui_smoke
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-ui-smoke-linux.sh
                         $<TARGET_FILE:CS80Snapshots> --out ${CMAKE_CURRENT_BINARY_DIR}/ui-smoke --self-test)
    else()
        add_test(NAME cs80_ui_smoke
                 COMMAND CS80Snapshots --out ${CMAKE_CURRENT_BINARY_DIR}/ui-smoke --self-test)
    endif()
    set_tests_properties(cs80_ui_smoke PROPERTIES TIMEOUT 240)

    if (CS80_BUILD_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Reuse the snapshot target, but dispatch before its ordinary GUI setup.
        # Each mode/fixture starts a fresh process: no preceding query or paint
        # can warm up JUCE and hide a locale mutation. Never skip a missing fixture.
        find_program(CS80_XVFB_RUN xvfb-run REQUIRED)
        find_program(CS80_XAUTH xauth REQUIRED)
        foreach(locale_mode IN ITEMS query editor)
            add_test(NAME cs80_locale_${locale_mode}
                     COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C.UTF-8 LANG=C.UTF-8
                             JUCE_FONT_PATH=/usr/share/fonts/truetype/dejavu
                             ${CS80_XVFB_RUN} -a -s "-screen 0 1920x1200x24"
                             $<TARGET_FILE:CS80Snapshots> --locale-smoke ${locale_mode})
            add_test(NAME cs80_locale_${locale_mode}_mixed
                     COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C.UTF-8 LANG=C.UTF-8
                             JUCE_FONT_PATH=/usr/share/fonts/truetype/dejavu
                             ${CS80_XVFB_RUN} -a -s "-screen 0 1920x1200x24"
                             $<TARGET_FILE:CS80Snapshots> --locale-smoke ${locale_mode} mixed)
            set_tests_properties(cs80_locale_${locale_mode} cs80_locale_${locale_mode}_mixed
                                 PROPERTIES TIMEOUT 60)
        endforeach()
    endif()
endif()
