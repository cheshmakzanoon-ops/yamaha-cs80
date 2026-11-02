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
    # Frozen scalar-kernel oracle: registered in c56726f before optimization.
    set(cs80_scalar_oracle_dir "${CMAKE_CURRENT_BINARY_DIR}/scalar-oracle/exact_scalar_frozen")
    set(cs80_scalar_oracle_inputs "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/exact_scalar_kernels/manifest.json")
    set(cs80_scalar_oracle_outputs "${cs80_scalar_oracle_dir}/oracle-source-manifest.json")
    foreach(cs80_scalar_header Dsp.h FastMath.h Ig00156PhaseCorrection.h Ig00156Filter.h ReconstructedOscillator.h)
        list(APPEND cs80_scalar_oracle_inputs
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/exact_scalar_kernels/raw/${cs80_scalar_header}")
        list(APPEND cs80_scalar_oracle_outputs "${cs80_scalar_oracle_dir}/${cs80_scalar_header}")
    endforeach()
    add_custom_command(OUTPUT ${cs80_scalar_oracle_outputs}
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu/materialize-frozen-scalar-kernels.py"
                --fixture-root "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/exact_scalar_kernels"
                --output "${cs80_scalar_oracle_dir}"
        DEPENDS ${cs80_scalar_oracle_inputs}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu/materialize-frozen-scalar-kernels.py"
        VERBATIM)
    add_custom_target(cs80_exact_scalar_oracle DEPENDS ${cs80_scalar_oracle_outputs})
    add_executable(cs80_exact_scalar_filter_tests tests/ExactScalarFilterTests.cpp)
    add_dependencies(cs80_exact_scalar_filter_tests cs80_exact_scalar_oracle)
    target_include_directories(cs80_exact_scalar_filter_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/scalar-oracle")
    foreach(cs80_scalar_suite fixed moving state row fp)
        add_test(NAME cs80_exact_scalar_filter_${cs80_scalar_suite}_tests
                 COMMAND cs80_exact_scalar_filter_tests --suite ${cs80_scalar_suite})
        set_tests_properties(cs80_exact_scalar_filter_${cs80_scalar_suite}_tests PROPERTIES TIMEOUT 300)
    endforeach()
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
    add_executable(cs80_cold_idle_timing_tests tests/ColdIdleTimingTests.cpp)
    target_link_libraries(cs80_cold_idle_timing_tests PRIVATE cs80_core)
    add_test(NAME cs80_cold_idle_timing_tests COMMAND cs80_cold_idle_timing_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/cold-idle-timing.json)
    set_tests_properties(cs80_cold_idle_timing_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_cold_filter_envelope_tests tests/ColdFilterEnvelopeTests.cpp)
    target_link_libraries(cs80_cold_filter_envelope_tests PRIVATE cs80_core)
    add_test(NAME cs80_cold_filter_envelope_tests COMMAND cs80_cold_filter_envelope_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/completion-dsp.json)
    set_tests_properties(cs80_cold_filter_envelope_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_note_off_continuity_tests tests/NoteOffContinuityTests.cpp)
    target_link_libraries(cs80_note_off_continuity_tests PRIVATE cs80_core)
    add_test(NAME cs80_note_off_continuity_tests COMMAND cs80_note_off_continuity_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/completion-dsp.json)
    set_tests_properties(cs80_note_off_continuity_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_click_event_continuation_tests tests/ClickEventContinuationTests.cpp)
    target_link_libraries(cs80_click_event_continuation_tests PRIVATE cs80_core)
    add_test(NAME cs80_click_event_continuation_tests COMMAND cs80_click_event_continuation_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/click-residual-continuity.json
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/cold-idle-timing.json)
    set_tests_properties(cs80_click_event_continuation_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_ring_event_continuity_tests tests/RingEventContinuityTests.cpp)
    target_link_libraries(cs80_ring_event_continuity_tests PRIVATE cs80_core)
    add_test(NAME cs80_ring_event_continuity_tests COMMAND cs80_ring_event_continuity_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ring-event-continuity.json)
    set_tests_properties(cs80_ring_event_continuity_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_ring_event_causality tests/RingEventCausality.cpp)
    target_link_libraries(cs80_ring_event_causality PRIVATE cs80_core)
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
    add_executable(cs80_chorus_tabs_tests tests/ChorusTabsTests.cpp)
    target_link_libraries(cs80_chorus_tabs_tests PRIVATE cs80_core)
    add_test(NAME cs80_chorus_tabs_tests COMMAND cs80_chorus_tabs_tests)

    add_executable(cs80_idle_exact_skip_tests tests/IdleExactSkipTests.cpp)
    target_include_directories(cs80_idle_exact_skip_tests PRIVATE src)
    add_test(NAME cs80_idle_exact_skip_tests COMMAND cs80_idle_exact_skip_tests)
    set_tests_properties(cs80_idle_exact_skip_tests PROPERTIES TIMEOUT 120)
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
    add_executable(cs80_oscillator_cache_tests tests/OscillatorCacheTests.cpp)
    target_link_libraries(cs80_oscillator_cache_tests PRIVATE cs80_core)
    add_test(NAME cs80_oscillator_cache_tests COMMAND cs80_oscillator_cache_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/kernel-cache.json)
    set_tests_properties(cs80_oscillator_cache_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_ig00156_cache_tests tests/Ig00156CacheTests.cpp)
    target_link_libraries(cs80_ig00156_cache_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00156_cache_tests COMMAND cs80_ig00156_cache_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/kernel-cache.json)
    set_tests_properties(cs80_ig00156_cache_tests PROPERTIES TIMEOUT 60)
    add_executable(cs80_ig00156_companion_history_tests tests/Ig00156CompanionHistoryTests.cpp)
    target_include_directories(cs80_ig00156_companion_history_tests PRIVATE src)
    add_test(NAME cs80_ig00156_companion_history_tests COMMAND cs80_ig00156_companion_history_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu/lazy-filter-histories.json)
    set_tests_properties(cs80_ig00156_companion_history_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_ig00156_companion_efficiency_tests COMMAND ${Python3_EXECUTABLE}
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu/run-lazy-filter-histories.py
             --mode efficiency --source ${CMAKE_CURRENT_SOURCE_DIR}
             --contract ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu/lazy-filter-histories.json
             --cmake ${CMAKE_COMMAND} --cxx ${CMAKE_CXX_COMPILER})
    set_tests_properties(cs80_ig00156_companion_efficiency_tests PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
    add_executable(cs80_ig00156_nodal_tests tests/Ig00156NodalTests.cpp)
    target_link_libraries(cs80_ig00156_nodal_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00156_nodal_tests COMMAND cs80_ig00156_nodal_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json)
    set_tests_properties(cs80_ig00156_nodal_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_ig00156_phase_correction_tests tests/Ig00156PhaseCorrectionTests.cpp)
    target_link_libraries(cs80_ig00156_phase_correction_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00156_phase_correction_tests COMMAND cs80_ig00156_phase_correction_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ig00156-176400.json)
    set_tests_properties(cs80_ig00156_phase_correction_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_oscillator_promotion_tests tests/OscillatorPromotionTests.cpp)
    target_link_libraries(cs80_oscillator_promotion_tests PRIVATE cs80_core)
    add_test(NAME cs80_oscillator_promotion_tests COMMAND cs80_oscillator_promotion_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json)
    set_tests_properties(cs80_oscillator_promotion_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_ig00151_vca_tests tests/Ig00151VcaTests.cpp)
    target_link_libraries(cs80_ig00151_vca_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00151_vca_tests COMMAND cs80_ig00151_vca_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json)
    set_tests_properties(cs80_ig00151_vca_tests PROPERTIES TIMEOUT 120)

    add_executable(cs80_ig00151_idle_tail_tests tests/Ig00151IdleTailTests.cpp)
    target_link_libraries(cs80_ig00151_idle_tail_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00151_idle_tail_tests COMMAND cs80_ig00151_idle_tail_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json)
    set_tests_properties(cs80_ig00151_idle_tail_tests PROPERTIES TIMEOUT 120)

    add_executable(cs80_vca_tail_modulation_tests tests/VcaTailModulationTests.cpp)
    target_link_libraries(cs80_vca_tail_modulation_tests PRIVATE cs80_core)
    add_test(NAME cs80_vca_tail_modulation_tests COMMAND cs80_vca_tail_modulation_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/vca-tail-modulation.json)
    set_tests_properties(cs80_vca_tail_modulation_tests PROPERTIES TIMEOUT 240)
    add_executable(cs80_modern_tail_scheduling_tests tests/ModernTailSchedulingTests.cpp)
    target_link_libraries(cs80_modern_tail_scheduling_tests PRIVATE cs80_core)
    add_test(NAME cs80_modern_tail_scheduling_tests COMMAND cs80_modern_tail_scheduling_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/vca-tail-modulation.json)
    add_executable(cs80_tolerance_drift_tests tests/ToleranceDriftTests.cpp)
    target_link_libraries(cs80_tolerance_drift_tests PRIVATE cs80_core)
    add_test(NAME cs80_tolerance_drift_tests COMMAND cs80_tolerance_drift_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json)
    set_tests_properties(cs80_tolerance_drift_tests PROPERTIES TIMEOUT 300)

    # [ws2-engine] register this workstream's core tests below this line.
    add_executable(cs80_oversampling_tests tests/OversamplingTests.cpp)
    target_link_libraries(cs80_oversampling_tests PRIVATE cs80_core)
    add_test(NAME cs80_oversampling_tests COMMAND cs80_oversampling_tests)
    add_executable(cs80_stationary_controls_exact_tests tests/StationaryControlsExactTests.cpp)
    target_include_directories(cs80_stationary_controls_exact_tests PRIVATE src)
    add_test(NAME cs80_stationary_controls_exact_tests COMMAND cs80_stationary_controls_exact_tests)
    set_tests_properties(cs80_stationary_controls_exact_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_smoothing_tests tests/SmoothingTests.cpp)
    target_link_libraries(cs80_smoothing_tests PRIVATE cs80_core)
    add_test(NAME cs80_smoothing_tests COMMAND cs80_smoothing_tests)
    add_executable(cs80_output_listening_tests tests/OutputListeningTests.cpp)
    target_link_libraries(cs80_output_listening_tests PRIVATE cs80_core)
    add_test(NAME cs80_output_listening_tests COMMAND cs80_output_listening_tests)
    add_executable(cs80_noise_tests tests/NoiseTests.cpp)
    target_link_libraries(cs80_noise_tests PRIVATE cs80_core)
    add_test(NAME cs80_noise_tests COMMAND cs80_noise_tests)

    # [ws3-keyboard] register this workstream's core tests below this line.
    add_executable(cs80_mts_esp_native_tests tests/MtsEspNativeTests.cpp src/plugin/MtsEspClient.cpp)
    target_include_directories(cs80_mts_esp_native_tests PRIVATE src)
    target_link_libraries(cs80_mts_esp_native_tests PRIVATE ${CMAKE_DL_LIBS})
    add_test(NAME cs80_mts_esp_native_tests COMMAND cs80_mts_esp_native_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/mts-client.json)
    set_tests_properties(cs80_mts_esp_native_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_mts_esp_shim_tests tests/MtsEspShimTests.cpp src/plugin/MtsEspClient.cpp)
    target_compile_definitions(cs80_mts_esp_shim_tests PRIVATE CS80_WITH_MTS_ESP=1 CS80_MTS_ESP_TEST_BACKEND=1)
    target_link_libraries(cs80_mts_esp_shim_tests PRIVATE cs80_core)
    add_test(NAME cs80_mts_esp_shim_tests COMMAND cs80_mts_esp_shim_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/mts-client.json)
    set_tests_properties(cs80_mts_esp_shim_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_mts_esp_fallback_tests tests/MtsEspFallbackTests.cpp src/plugin/MtsEspClient.cpp)
    target_include_directories(cs80_mts_esp_fallback_tests PRIVATE src)
    target_compile_definitions(cs80_mts_esp_fallback_tests PRIVATE CS80_WITH_MTS_ESP=0)
    add_test(NAME cs80_mts_esp_fallback_tests COMMAND cs80_mts_esp_fallback_tests)
    add_executable(cs80_mts_esp_windows_macro_tests tests/MtsEspWindowsMacroTests.cpp)
    target_include_directories(cs80_mts_esp_windows_macro_tests PRIVATE src)
    target_compile_definitions(cs80_mts_esp_windows_macro_tests PRIVATE
        CS80_WITH_MTS_ESP=1 CS80_MTS_ESP_TEST_BACKEND=1)
    add_test(NAME cs80_mts_esp_windows_macro_tests COMMAND cs80_mts_esp_windows_macro_tests)
    add_executable(cs80_host_automation_curve_tests tests/HostAutomationCurveTests.cpp)
    target_include_directories(cs80_host_automation_curve_tests PRIVATE src)
    add_test(NAME cs80_host_automation_curve_tests COMMAND cs80_host_automation_curve_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json")
    add_executable(cs80_au_automation_curve_tests tests/AuAutomationCurveTests.cpp)
    target_include_directories(cs80_au_automation_curve_tests PRIVATE src)
    add_test(NAME cs80_au_automation_curve_tests COMMAND cs80_au_automation_curve_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json")
    add_executable(cs80_au_automation_bridge_contract_tests tests/AuAutomationBridgeContractTests.cpp)
    target_include_directories(cs80_au_automation_bridge_contract_tests PRIVATE src)
    target_link_libraries(cs80_au_automation_bridge_contract_tests PRIVATE Threads::Threads)
    add_test(NAME cs80_au_automation_bridge_contract_tests COMMAND cs80_au_automation_bridge_contract_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json")
    set_tests_properties(cs80_mts_esp_fallback_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_free_running_voice_tests tests/FreeRunningVoiceTests.cpp)
    target_link_libraries(cs80_free_running_voice_tests PRIVATE cs80_core)
    add_test(NAME cs80_free_running_voice_tests COMMAND cs80_free_running_voice_tests)
    set_tests_properties(cs80_free_running_voice_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_kas_scanner_tests tests/KasScannerTests.cpp)
    target_link_libraries(cs80_kas_scanner_tests PRIVATE cs80_core)
    add_test(NAME cs80_kas_scanner_tests COMMAND cs80_kas_scanner_tests)
    set_tests_properties(cs80_kas_scanner_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_keyboard_contour_key_map_tests tests/KeyboardContourKeyMapTests.cpp)
    target_link_libraries(cs80_keyboard_contour_key_map_tests PRIVATE cs80_core)
    add_test(NAME cs80_keyboard_contour_key_map_tests COMMAND cs80_keyboard_contour_key_map_tests)
    set_tests_properties(cs80_keyboard_contour_key_map_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_touch_circuit_tests tests/TouchCircuitTests.cpp)
    target_link_libraries(cs80_touch_circuit_tests PRIVATE cs80_core)
    add_test(NAME cs80_touch_circuit_tests COMMAND cs80_touch_circuit_tests)
    set_tests_properties(cs80_touch_circuit_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_tuning_midi_tests tests/TuningMidiTests.cpp)
    target_link_libraries(cs80_tuning_midi_tests PRIVATE cs80_core)
    target_compile_definitions(cs80_tuning_midi_tests PRIVATE
        CS80_TUNING_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/tuning")
    add_test(NAME cs80_tuning_midi_tests COMMAND cs80_tuning_midi_tests)
    set_tests_properties(cs80_tuning_midi_tests PROPERTIES TIMEOUT 120)

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
    add_executable(cs80_unused_lfo_readout_exact_tests tests/UnusedLfoReadoutExactTests.cpp)
    target_include_directories(cs80_unused_lfo_readout_exact_tests PRIVATE src)
    add_test(NAME cs80_unused_lfo_readout_exact_tests COMMAND cs80_unused_lfo_readout_exact_tests)
    set_tests_properties(cs80_unused_lfo_readout_exact_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_unused_lfo_engine_ownership_tests tests/UnusedLfoEngineOwnershipTests.cpp)
    target_link_libraries(cs80_unused_lfo_engine_ownership_tests PRIVATE cs80_core)
    add_test(NAME cs80_unused_lfo_engine_ownership_tests COMMAND cs80_unused_lfo_engine_ownership_tests)
    set_tests_properties(cs80_unused_lfo_engine_ownership_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_modern_lfo_tests tests/ModernLfoTests.cpp)
    target_link_libraries(cs80_modern_lfo_tests PRIVATE cs80_core)
    add_test(NAME cs80_modern_lfo_tests COMMAND cs80_modern_lfo_tests)
    set_tests_properties(cs80_modern_lfo_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mseg_tests tests/MsegTests.cpp)
    target_include_directories(cs80_mseg_tests PRIVATE src)
    add_test(NAME cs80_mseg_tests COMMAND cs80_mseg_tests)
    set_tests_properties(cs80_mseg_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mod_matrix_tests tests/ModMatrixTests.cpp)
    target_link_libraries(cs80_mod_matrix_tests PRIVATE cs80_core)
    add_test(NAME cs80_mod_matrix_tests COMMAND cs80_mod_matrix_tests)
    set_tests_properties(cs80_mod_matrix_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_modern_fx_tests tests/ModernFxTests.cpp)
    target_link_libraries(cs80_modern_fx_tests PRIVATE cs80_core)
    add_test(NAME cs80_modern_fx_tests COMMAND cs80_modern_fx_tests)
    set_tests_properties(cs80_modern_fx_tests PROPERTIES TIMEOUT 900)

    # [ws6-library] register this workstream's core tests below this line.
    find_package(Threads REQUIRED)
    add_executable(cs80_library_audit_tests tests/LibraryAuditTests.cpp)
    target_link_libraries(cs80_library_audit_tests PRIVATE cs80_core Threads::Threads)
    add_test(NAME cs80_library_audit_tests COMMAND cs80_library_audit_tests)
    set_tests_properties(cs80_library_audit_tests PROPERTIES TIMEOUT 600)
    add_executable(cs80_randomise_tests tests/RandomiseTests.cpp)
    target_link_libraries(cs80_randomise_tests PRIVATE cs80_core)
    add_test(NAME cs80_randomise_tests COMMAND cs80_randomise_tests)
    add_executable(cs80_source_factory_boundary_tests tests/SourceFactoryBoundaryTests.cpp)
    target_include_directories(cs80_source_factory_boundary_tests PRIVATE src)
    add_test(NAME cs80_source_factory_boundary_tests
             COMMAND cs80_source_factory_boundary_tests
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws6-library.json)
    add_executable(cs80_programmer_integration_tests tests/ProgrammerIntegrationTests.cpp)
    target_link_libraries(cs80_programmer_integration_tests PRIVATE cs80_core)
    add_test(NAME cs80_programmer_integration_tests COMMAND cs80_programmer_integration_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/programmer-integration.json)
    set_tests_properties(cs80_programmer_integration_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_memory_panel_tests tests/MemoryPanelTests.cpp)
    target_include_directories(cs80_memory_panel_tests PRIVATE src)
    add_test(NAME cs80_memory_panel_tests
             COMMAND cs80_memory_panel_tests
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws6-library.json)

    # [ws7-panel] register this workstream's core tests below this line.

    # [integration] CPU pass: bounded-error approximations against std::.
    add_executable(cs80_fast_math_tests tests/FastMathTests.cpp)
    target_link_libraries(cs80_fast_math_tests PRIVATE cs80_core)
    add_test(NAME cs80_fast_math_tests COMMAND cs80_fast_math_tests)
    set_tests_properties(cs80_fast_math_tests PROPERTIES TIMEOUT 60)

    # [ws8-studio] register this workstream's core tests below this line.
    add_executable(cs80_evidence_register_tests tests/EvidenceRegisterTests.cpp)
    target_include_directories(cs80_evidence_register_tests PRIVATE src)
    add_test(NAME cs80_evidence_register_tests
             COMMAND cs80_evidence_register_tests ${CMAKE_CURRENT_SOURCE_DIR}/docs)
    set_tests_properties(cs80_evidence_register_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_midi_learn_tests tests/MidiLearnTests.cpp)
    target_include_directories(cs80_midi_learn_tests PRIVATE src)
    target_link_libraries(cs80_midi_learn_tests PRIVATE Threads::Threads)
    add_test(NAME cs80_midi_learn_tests COMMAND cs80_midi_learn_tests)
    set_tests_properties(cs80_midi_learn_tests PROPERTIES TIMEOUT 60)
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

    include(cmake/HostAutomation.cmake)

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
        src/plugin/ParameterContext.cpp
        src/plugin/AdvancedPanel.cpp
        src/plugin/PresetBrowser.cpp

        # [ws1-voice] add this workstream's plugin sources below this line.

        # [ws2-engine] add this workstream's plugin sources below this line.

        # [ws3-keyboard] add this workstream's plugin sources below this line.
        src/plugin/MtsEspClient.cpp
        src/plugin/PluginProcessorAutomation.cpp
        src/plugin/KeyboardTuningPage.cpp

        # [ws4-circuits] add this workstream's plugin sources below this line.

        # [ws5-modern] add this workstream's plugin sources below this line.
        src/plugin/ModernLayerState.cpp
        src/plugin/ModernModulationUi.cpp

        # [ws6-library] add this workstream's plugin sources below this line.
        src/plugin/PluginProcessorLibrary.cpp

        # [ws7-panel] add this workstream's plugin sources below this line.
        src/plugin/ArtCommon.cpp
        src/plugin/PanelWidgets.cpp
        src/plugin/ChannelRow.cpp
        src/plugin/CentreRow.cpp
        src/plugin/PerformanceDeck.cpp
        src/plugin/MemoryLid.cpp
        src/plugin/EditorPreferences.cpp

        # [ws8-studio] add this workstream's plugin sources below this line.
        src/plugin/StudioToolbar.cpp
        src/plugin/StudioDrawers.cpp
        src/plugin/UndoHistory.cpp
        src/plugin/StudioTour.cpp

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

    if(CS80_BUILD_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        get_target_property(CS80_JUCE_MODULES juce_audio_processors_headless INTERFACE_JUCE_MODULE_PATH)
        set(CS80_VST3_SDK "${CS80_JUCE_MODULES}/juce_audio_processors_headless/format_types/VST3_SDK")
        add_executable(cs80_vst3_automation_tests tests/Vst3AutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_automation_tests CS80Reference_VST3)
        add_test(NAME cs80_vst3_automation_tests COMMAND cs80_vst3_automation_tests
            "$<TARGET_FILE:CS80Reference_VST3>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json")
        set_tests_properties(cs80_vst3_automation_tests PROPERTIES TIMEOUT 180)
    endif()

    if(CS80_BUILD_TESTS AND APPLE)
        add_executable(cs80_au_automation_host_tests tests/AuAutomationHostTests.cpp)
        target_include_directories(cs80_au_automation_host_tests PRIVATE src)
        target_link_libraries(cs80_au_automation_host_tests PRIVATE
            "-framework AudioToolbox" "-framework AudioUnit" "-framework CoreFoundation")
        add_dependencies(cs80_au_automation_host_tests CS80Reference_AU)
        add_test(NAME cs80_au_automation_host_tests COMMAND cs80_au_automation_host_tests
            "$<TARGET_BUNDLE_DIR:CS80Reference_AU>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json")
        set_tests_properties(cs80_au_automation_host_tests PROPERTIES TIMEOUT 180)
        add_test(NAME cs80_au_automation_native_mutations COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/AuAutomationNativeMutations.py"
            --source "${CMAKE_CURRENT_SOURCE_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}"
            --host "$<TARGET_FILE:cs80_au_automation_host_tests>"
            --component "$<TARGET_BUNDLE_DIR:CS80Reference_AU>"
            --tolerances "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
            --config "$<CONFIG>")
        set_tests_properties(cs80_au_automation_native_mutations PROPERTIES TIMEOUT 1200 RUN_SERIAL TRUE
            DEPENDS cs80_au_automation_host_tests)
    endif()

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

    # Phase 7 validator diagnosis: one module life cycle (JUCE start, instance and editor paint,
    # destruction, JUCE shutdown, then static destructors) must exit cleanly. CI runs it plainly;
    # docs/VALIDATOR_DIAGNOSTICS.md records the memcheck run of the same probe.
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME cs80_teardown_probe
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-ui-smoke-linux.sh
                         $<TARGET_FILE:CS80Snapshots> --teardown-probe)
    else()
        add_test(NAME cs80_teardown_probe COMMAND CS80Snapshots --teardown-probe)
    endif()
    set_tests_properties(cs80_teardown_probe PROPERTIES TIMEOUT 120
                         PASS_REGULAR_EXPRESSION "JUCE shut down; static destructors run")

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

# AU wrapper macro environment contract (registered 2026-10-31).
# Keep the native JUCE macros active around the production queue on every host.
# Native macOS AU compilation, scheduling and pluginval remain independent gates.
if (CS80_BUILD_TESTS)
    add_executable(cs80_au_wrapper_macro_environment_tests tests/AuWrapperMacroEnvironmentTests.cpp)
    target_include_directories(cs80_au_wrapper_macro_environment_tests PRIVATE src)
    add_test(NAME cs80_au_wrapper_macro_environment_tests COMMAND cs80_au_wrapper_macro_environment_tests)
    set_tests_properties(cs80_au_wrapper_macro_environment_tests PROPERTIES TIMEOUT 30)
endif()

# Exact zero-history FIR gates; published acceptance and rounding extension in
# 156d82e9f3ee20089bb952ba853e0f0c779b903f. No production-wide FP option changes.
if (CS80_BUILD_TESTS)
    add_executable(cs80_zero_fir_history_tests tests/ZeroFirHistoryTests.cpp)
    target_include_directories(cs80_zero_fir_history_tests PRIVATE src)
    add_test(NAME cs80_zero_fir_history_tests COMMAND cs80_zero_fir_history_tests)
    set_tests_properties(cs80_zero_fir_history_tests PROPERTIES TIMEOUT 60)
    add_executable(cs80_zero_fir_rounding_tests tests/ZeroFirRoundingTests.cpp)
    target_include_directories(cs80_zero_fir_rounding_tests PRIVATE src)
    if (MSVC)
        target_compile_options(cs80_zero_fir_rounding_tests PRIVATE /fp:strict)
    else()
        target_compile_options(cs80_zero_fir_rounding_tests PRIVATE -frounding-math)
    endif()
    add_test(NAME cs80_zero_fir_rounding_tests COMMAND cs80_zero_fir_rounding_tests)
    set_tests_properties(cs80_zero_fir_rounding_tests PROPERTIES TIMEOUT 120)
endif()

# Exact control-transform gate; registered before implementation in c881983.
if (CS80_BUILD_TESTS)
    add_executable(cs80_exact_control_transform_tests tests/ExactControlTransformTests.cpp)
    target_link_libraries(cs80_exact_control_transform_tests PRIVATE cs80_core)
    add_test(NAME cs80_exact_control_transform_tests COMMAND cs80_exact_control_transform_tests)
endif()

# Core test executables keep several Engine fixtures (about 270 KB each since
# the free-running idle snapshots and the circuit-engine voice) on the stack in
# one function. Give them the ordinary 8 MiB Unix test-process budget instead of
# MSVC's 1 MiB executable default, as CS80Snapshots already has. Not applied to
# the shipping plugin or Standalone targets.
if (MSVC AND CS80_BUILD_TESTS)
    get_property(cs80_all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(cs80_target IN LISTS cs80_all_targets)
        if (cs80_target MATCHES "^cs80_.*tests$")
            get_target_property(cs80_target_type ${cs80_target} TYPE)
            if (cs80_target_type STREQUAL "EXECUTABLE")
                target_link_options(${cs80_target} PRIVATE /STACK:8388608)
            endif()
        endif()
    endforeach()
endif()

# Host automation wrapper platform contract (registered 2026-10-31).
# Mock interface source lists exercise C++ and Objective-C++ generation on
# every build host; native AU/VST3 compilation and host gates remain separate.
if (CS80_BUILD_PLUGIN AND CS80_BUILD_TESTS)
    get_target_property(CS80_WRAPPER_JUCE_MODULES juce_audio_processors_headless INTERFACE_JUCE_MODULE_PATH)
    add_test(NAME cs80_host_wrapper_platform_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/HostWrapperPlatformTests.py"
        --source "${CMAKE_CURRENT_SOURCE_DIR}"
        --juce-wrappers "${CS80_WRAPPER_JUCE_MODULES}/juce_audio_plugin_client"
        --tolerances "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-wrapper-platforms.json"
        --cmake "${CMAKE_COMMAND}")
    set_tests_properties(cs80_host_wrapper_platform_tests PROPERTIES TIMEOUT 90)
endif()

# Instrumented builds (ASan + UBSan) run several times slower than the Release
# builds the per-test TIMEOUT values were chosen for: under the sanitizers the
# merged suite's envelope-precision, source-factory-integration and library
# audit executables outlive their limits without any sanitizer finding.
# CS80_TEST_TIMEOUT_SCALE multiplies every test's TIMEOUT; it changes no
# assertion. dsp-measurements.yml's sanitizer job sets it to 4.
set(CS80_TEST_TIMEOUT_SCALE 1 CACHE STRING "Multiplier for every test TIMEOUT (instrumented builds)")
if (CS80_BUILD_TESTS AND NOT CS80_TEST_TIMEOUT_SCALE EQUAL 1)
    get_property(cs80_all_tests DIRECTORY PROPERTY TESTS)
    foreach(cs80_test IN LISTS cs80_all_tests)
        get_test_property(${cs80_test} TIMEOUT cs80_timeout)
        if (cs80_timeout)
            math(EXPR cs80_timeout "${cs80_timeout} * ${CS80_TEST_TIMEOUT_SCALE}")
            set_tests_properties(${cs80_test} PROPERTIES TIMEOUT ${cs80_timeout})
        endif()
    endforeach()
endif()
