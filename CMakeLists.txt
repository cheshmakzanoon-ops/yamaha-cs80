cmake_minimum_required(VERSION 3.22)
project(CS80Reference VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CS80_BUILD_PLUGIN "Build the JUCE plugin and standalone application" ON)
option(CS80_BUILD_TESTS "Build core tests" ON)
option(CS80_ENABLE_AVX2_FILTER_BATCH "Opt into exact GCC AVX2 stage batches (throughput qualification is workload-specific)" OFF)

add_library(cs80_core STATIC
    src/core/Engine.cpp
    src/core/FactoryPresets.cpp
    src/core/Ig00156Batch.cpp
)
# Do not alter the existing scalar arithmetic contract. The optional AVX2
# function is isolated; dispatch and every unsupported build remain baseline ISA.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(src/core/Ig00156Batch.cpp PROPERTIES
        COMPILE_OPTIONS "-ffp-contract=off")
endif()
target_include_directories(cs80_core PUBLIC src)
if (NOT CS80_ENABLE_AVX2_FILTER_BATCH OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Unknown/Clang math contracts fail closed. This only disables the optional
    # backend; it never overrides any scalar source's floating-point flags.
    target_compile_definitions(cs80_core PUBLIC CS80_DISABLE_AVX2_FILTER_BATCH=1)
    if (CS80_ENABLE_AVX2_FILTER_BATCH AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "AVX2 filter batch request remains scalar: only GCC has a qualified detectable math contract")
    endif()
endif()
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
    add_executable(cs80_legacy_sub_lfo_oracle_tests tests/LegacySubLfoOracleTests.cpp)
    target_link_libraries(cs80_legacy_sub_lfo_oracle_tests PRIVATE cs80_core)
    add_test(NAME cs80_legacy_sub_lfo_oracle_tests COMMAND cs80_legacy_sub_lfo_oracle_tests)
    set_tests_properties(cs80_legacy_sub_lfo_oracle_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_ig00156_batch_tests tests/Ig00156BatchTests.cpp)
    target_link_libraries(cs80_ig00156_batch_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00156_batch_tests COMMAND cs80_ig00156_batch_tests)
    set_tests_properties(cs80_ig00156_batch_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_filter_batch_engine_tests tests/FilterBatchEngineTests.cpp)
    target_link_libraries(cs80_filter_batch_engine_tests PRIVATE cs80_core)
    add_test(NAME cs80_filter_batch_engine_tests COMMAND cs80_filter_batch_engine_tests)
    set_tests_properties(cs80_filter_batch_engine_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_filter_batch_scheduling_tests tests/FilterBatchSchedulingTests.cpp)
    target_link_libraries(cs80_filter_batch_scheduling_tests PRIVATE cs80_core)
    add_test(NAME cs80_filter_batch_scheduling_tests COMMAND cs80_filter_batch_scheduling_tests)
    set_tests_properties(cs80_filter_batch_scheduling_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_ig00151_zero_ei_tests tests/Ig00151ZeroEiTests.cpp)
    target_include_directories(cs80_ig00151_zero_ei_tests PRIVATE src)
    add_test(NAME cs80_ig00151_zero_ei_tests COMMAND cs80_ig00151_zero_ei_tests)
    set_tests_properties(cs80_ig00151_zero_ei_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_event_timeline_tests tests/EventTimelineTests.cpp)
    target_include_directories(cs80_event_timeline_tests PRIVATE src)
    add_test(NAME cs80_event_timeline_tests COMMAND cs80_event_timeline_tests)
    set_tests_properties(cs80_event_timeline_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_pump_until_tests tests/PumpUntilTests.cpp)
    add_test(NAME cs80_pump_until_tests COMMAND cs80_pump_until_tests)
    set_tests_properties(cs80_pump_until_tests PROPERTIES TIMEOUT 30)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    if (NOT MSVC AND NOT CMAKE_CROSSCOMPILING AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        add_test(NAME cs80_filter_batch_admission_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_filter_batch_admission.py
                         --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(cs80_filter_batch_admission_tests PROPERTIES TIMEOUT 180)
    endif()
    # Inert cross-platform artifact/header and notice regressions. These must
    # run with ordinary CTest, not only when a packaging helper is run by hand.
    add_test(NAME cs80_package_binary_identity_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-package-binary-identity.py)
    add_test(NAME cs80_packaging_notice_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-packaging-notices.py)
    set_tests_properties(cs80_package_binary_identity_tests cs80_packaging_notice_tests
                         PROPERTIES TIMEOUT 60 LABELS "packaging")
    add_test(NAME cs80_ci_juce_provenance_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-ci-juce-provenance.py
                     --cmake ${CMAKE_COMMAND})
    set_tests_properties(cs80_ci_juce_provenance_tests PROPERTIES TIMEOUT 60 LABELS "packaging")
    add_test(NAME cs80_pluginval_windows_download_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-pluginval-windows-download.py)
    set_tests_properties(cs80_pluginval_windows_download_tests PROPERTIES TIMEOUT 30)
    add_test(NAME cs80_pluginval_macos_download_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-pluginval-macos-download.py)
    set_tests_properties(cs80_pluginval_macos_download_tests PROPERTIES TIMEOUT 30)
    # The Debian fixture packager accepts native amd64 ELF only. Other hosts
    # still exercise all archive formats and installer notice source mappings.
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND
        CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        add_test(NAME cs80_deb_runtime_dependency_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-deb-runtime-dependencies.py)
        set_tests_properties(cs80_deb_runtime_dependency_tests PROPERTIES TIMEOUT 60 LABELS "packaging")
    endif()
    add_test(NAME cs80_ci_bundle_verification_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-verify-ci-bundle.py)
    set_tests_properties(cs80_ci_bundle_verification_tests PROPERTIES TIMEOUT 30)
    add_test(NAME cs80_windows_installer_preflight_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-windows-installer-preflight.py)
    set_tests_properties(cs80_windows_installer_preflight_tests PROPERTIES TIMEOUT 180)
    add_test(NAME cs80_parity_harness_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parity_harness.py)
    set_tests_properties(cs80_parity_harness_tests PROPERTIES TIMEOUT 60)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME cs80_ui_environment_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ui_smoke_environment.py)
        set_tests_properties(cs80_ui_environment_tests PROPERTIES TIMEOUT 45)
    endif()
    if (UNIX AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        # Compile the actual delete handler against inert doubles with UBSan.
        # No native trash service, GUI, or user preset is touched by this test.
        add_test(NAME cs80_preset_trash_safety_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-preset-trash-safety.py
                         --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(cs80_preset_trash_safety_tests PROPERTIES TIMEOUT 60)
        # Exact preview arithmetic with only an inert pi constant in place of JUCE.
        # This checks finite geometry inputs, not GUI rendering or preset loading.
        add_test(NAME cs80_preset_signature_math_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-preset-signature-math.py
                         --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(cs80_preset_signature_math_tests PROPERTIES TIMEOUT 120)
    endif()
    if (UNIX)
        # Synthetic shell/tool fixtures only; no native installer or product runs.
        add_test(NAME cs80_macos_installer_architecture_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-macos-installer-architectures.py)
        set_tests_properties(cs80_macos_installer_architecture_tests PROPERTIES TIMEOUT 60)
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
    add_executable(cs80_lfo_scope_preview_tests tests/LfoScopePreviewTests.cpp)
    target_include_directories(cs80_lfo_scope_preview_tests PRIVATE src)
    add_test(NAME cs80_lfo_scope_preview_tests COMMAND cs80_lfo_scope_preview_tests)
    set_tests_properties(cs80_lfo_scope_preview_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_preset_preview_math_tests tests/PresetPreviewMathTests.cpp)
    target_include_directories(cs80_preset_preview_math_tests PRIVATE src)
    add_test(NAME cs80_preset_preview_math_tests COMMAND cs80_preset_preview_math_tests)
    set_tests_properties(cs80_preset_preview_math_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_output_meter_display_tests tests/OutputMeterDisplayTests.cpp)
    target_include_directories(cs80_output_meter_display_tests PRIVATE src)
    add_test(NAME cs80_output_meter_display_tests COMMAND cs80_output_meter_display_tests)
    set_tests_properties(cs80_output_meter_display_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_voice_activity_tests tests/VoiceActivityTests.cpp)
    target_include_directories(cs80_voice_activity_tests PRIVATE src)
    target_link_libraries(cs80_voice_activity_tests PRIVATE Threads::Threads)
    add_test(NAME cs80_voice_activity_tests COMMAND cs80_voice_activity_tests)
    set_tests_properties(cs80_voice_activity_tests PROPERTIES TIMEOUT 30)
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
    add_executable(cs80_ig00156_cache_tests tests/Ig00156CacheTests.cpp)
    target_include_directories(cs80_ig00156_cache_tests PRIVATE src)
    add_test(NAME cs80_ig00156_cache_tests COMMAND cs80_ig00156_cache_tests)
    set_tests_properties(cs80_ig00156_cache_tests PROPERTIES TIMEOUT 60)
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
    add_executable(cs80_voice_release_tests tests/VoiceReleaseTests.cpp)
    target_link_libraries(cs80_voice_release_tests PRIVATE cs80_core)
    add_test(NAME cs80_voice_release_tests COMMAND cs80_voice_release_tests)
    set_tests_properties(cs80_voice_release_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_sixteen_voice_release_tests tests/SixteenVoiceReleaseTests.cpp)
    target_link_libraries(cs80_sixteen_voice_release_tests PRIVATE cs80_core)
    add_test(NAME cs80_sixteen_voice_release_tests COMMAND cs80_sixteen_voice_release_tests)
    set_tests_properties(cs80_sixteen_voice_release_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_unison_glide_tests tests/UnisonGlideTests.cpp)
    target_link_libraries(cs80_unison_glide_tests PRIVATE cs80_core)
    add_test(NAME cs80_unison_glide_tests COMMAND cs80_unison_glide_tests)
    set_tests_properties(cs80_unison_glide_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_pedal_release_tests tests/PedalReleaseTests.cpp)
    target_link_libraries(cs80_pedal_release_tests PRIVATE cs80_core)
    add_test(NAME cs80_pedal_release_tests COMMAND cs80_pedal_release_tests)
    set_tests_properties(cs80_pedal_release_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_high_resolution_velocity_tests tests/HighResolutionVelocityTests.cpp)
    target_include_directories(cs80_high_resolution_velocity_tests PRIVATE src)
    add_test(NAME cs80_high_resolution_velocity_tests COMMAND cs80_high_resolution_velocity_tests)
    set_tests_properties(cs80_high_resolution_velocity_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_controller_reset_tests tests/ControllerResetTests.cpp)
    target_link_libraries(cs80_controller_reset_tests PRIVATE cs80_core)
    add_test(NAME cs80_controller_reset_tests COMMAND cs80_controller_reset_tests)
    set_tests_properties(cs80_controller_reset_tests PROPERTIES TIMEOUT 120)
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
    add_test(NAME cs80_validator_diagnostic_selection_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-pluginval-diagnostic-log.py)
    set_tests_properties(cs80_validator_diagnostic_selection_tests PROPERTIES TIMEOUT 30)
    if (UNIX)
        add_test(NAME cs80_validator_retry_tests
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-pluginval-retry.sh
                              ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-pluginval-linux.sh)
        set_tests_properties(cs80_validator_retry_tests PROPERTIES TIMEOUT 30)
        add_test(NAME cs80_validator_exit_classification_tests
                 COMMAND ${Python3_EXECUTABLE}
                         ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-pluginval-exit-classification.py)
        set_tests_properties(cs80_validator_exit_classification_tests PROPERTIES TIMEOUT 30)
    endif()

    # [ws1-voice] register this workstream's core tests below this line.
    add_executable(cs80_ig00156_nodal_tests tests/Ig00156NodalTests.cpp)
    target_link_libraries(cs80_ig00156_nodal_tests PRIVATE cs80_core)
    add_test(NAME cs80_ig00156_nodal_tests COMMAND cs80_ig00156_nodal_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json)
    set_tests_properties(cs80_ig00156_nodal_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_ig00156_top_octave_tests tests/Ig00156TopOctaveTests.cpp)
    target_include_directories(cs80_ig00156_top_octave_tests PRIVATE src)
    if (MSVC)
        target_compile_options(cs80_ig00156_top_octave_tests PRIVATE /W4)
    else()
        target_compile_options(cs80_ig00156_top_octave_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cs80_ig00156_top_octave_tests COMMAND cs80_ig00156_top_octave_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/known-deviations/ig00156-top-octave.json)
    set_tests_properties(cs80_ig00156_top_octave_tests PROPERTIES TIMEOUT 120)
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
    add_executable(cs80_tolerance_drift_tests tests/ToleranceDriftTests.cpp)
    target_link_libraries(cs80_tolerance_drift_tests PRIVATE cs80_core)
    add_test(NAME cs80_tolerance_drift_tests COMMAND cs80_tolerance_drift_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws1-voice.json)
    set_tests_properties(cs80_tolerance_drift_tests PROPERTIES TIMEOUT 300)

    # [ws2-engine] register this workstream's core tests below this line.
    add_executable(cs80_external_interpolator_tests tests/ExternalModulationInterpolatorTests.cpp)
    target_include_directories(cs80_external_interpolator_tests PRIVATE src)
    add_test(NAME cs80_external_interpolator_tests COMMAND cs80_external_interpolator_tests)
    set_tests_properties(cs80_external_interpolator_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_oversampling_tests tests/OversamplingTests.cpp)
    target_link_libraries(cs80_oversampling_tests PRIVATE cs80_core)
    add_test(NAME cs80_oversampling_tests COMMAND cs80_oversampling_tests)
    add_executable(cs80_smoothing_tests tests/SmoothingTests.cpp)
    target_link_libraries(cs80_smoothing_tests PRIVATE cs80_core)
    add_test(NAME cs80_smoothing_tests COMMAND cs80_smoothing_tests)
    add_executable(cs80_pitch_bend_range_tests tests/PitchBendRangeTests.cpp)
    target_link_libraries(cs80_pitch_bend_range_tests PRIVATE cs80_core)
    add_test(NAME cs80_pitch_bend_range_tests COMMAND cs80_pitch_bend_range_tests)
    set_tests_properties(cs80_pitch_bend_range_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mpe_member_onset_tests tests/MpeMemberOnsetTests.cpp)
    target_link_libraries(cs80_mpe_member_onset_tests PRIVATE cs80_core)
    add_test(NAME cs80_mpe_member_onset_tests COMMAND cs80_mpe_member_onset_tests)
    set_tests_properties(cs80_mpe_member_onset_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mpe_timbre_combination_tests tests/MpeTimbreCombinationTests.cpp)
    target_link_libraries(cs80_mpe_timbre_combination_tests PRIVATE cs80_core)
    add_test(NAME cs80_mpe_timbre_combination_tests COMMAND cs80_mpe_timbre_combination_tests)
    set_tests_properties(cs80_mpe_timbre_combination_tests PROPERTIES TIMEOUT 120)

    add_executable(cs80_mpe_pressure_combination_tests tests/MpePressureCombinationTests.cpp)
    target_link_libraries(cs80_mpe_pressure_combination_tests PRIVATE cs80_core)
    add_test(NAME cs80_mpe_pressure_combination_tests COMMAND cs80_mpe_pressure_combination_tests)
    set_tests_properties(cs80_mpe_pressure_combination_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mpe_released_target_tests tests/MpeReleasedTargetTests.cpp)
    target_link_libraries(cs80_mpe_released_target_tests PRIVATE cs80_core)
    add_test(NAME cs80_mpe_released_target_tests COMMAND cs80_mpe_released_target_tests)
    set_tests_properties(cs80_mpe_released_target_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mpe_master_smoothing_tests tests/MpeMasterSmoothingTests.cpp)
    target_link_libraries(cs80_mpe_master_smoothing_tests PRIVATE cs80_core)
    add_test(NAME cs80_mpe_master_smoothing_tests COMMAND cs80_mpe_master_smoothing_tests)
    set_tests_properties(cs80_mpe_master_smoothing_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_output_listening_tests tests/OutputListeningTests.cpp)
    target_link_libraries(cs80_output_listening_tests PRIVATE cs80_core)
    add_test(NAME cs80_output_listening_tests COMMAND cs80_output_listening_tests)
    add_executable(cs80_noise_tests tests/NoiseTests.cpp)
    target_link_libraries(cs80_noise_tests PRIVATE cs80_core)
    add_test(NAME cs80_noise_tests COMMAND cs80_noise_tests)

    # [ws3-keyboard] register this workstream's core tests below this line.
    add_executable(cs80_poly_pressure_routing_tests tests/PolyPressureRoutingTests.cpp)
    target_link_libraries(cs80_poly_pressure_routing_tests PRIVATE cs80_core)
    add_test(NAME cs80_poly_pressure_routing_tests COMMAND cs80_poly_pressure_routing_tests)
    set_tests_properties(cs80_poly_pressure_routing_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_host_automation_curve_tests tests/HostAutomationCurveTests.cpp)
    target_include_directories(cs80_host_automation_curve_tests PRIVATE src)
    add_test(NAME cs80_host_automation_curve_tests COMMAND cs80_host_automation_curve_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-pan-v1.json")
    set_tests_properties(cs80_host_automation_curve_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_free_running_voice_tests tests/FreeRunningVoiceTests.cpp)
    target_link_libraries(cs80_free_running_voice_tests PRIVATE cs80_core)
    add_test(NAME cs80_free_running_voice_tests COMMAND cs80_free_running_voice_tests)
    set_tests_properties(cs80_free_running_voice_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_kas_scanner_tests tests/KasScannerTests.cpp)
    target_link_libraries(cs80_kas_scanner_tests PRIVATE cs80_core)
    add_test(NAME cs80_kas_scanner_tests COMMAND cs80_kas_scanner_tests)
    set_tests_properties(cs80_kas_scanner_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_scanner_ordering_tests tests/ScannerOrderingTests.cpp)
    target_link_libraries(cs80_scanner_ordering_tests PRIVATE cs80_core)
    add_test(NAME cs80_scanner_ordering_tests COMMAND cs80_scanner_ordering_tests)
    set_tests_properties(cs80_scanner_ordering_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_scanner_overflow_tests tests/ScannerOverflowTests.cpp)
    target_link_libraries(cs80_scanner_overflow_tests PRIVATE cs80_core)
    add_test(NAME cs80_scanner_overflow_tests COMMAND cs80_scanner_overflow_tests)
    set_tests_properties(cs80_scanner_overflow_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_keyboard_contour_key_map_tests tests/KeyboardContourKeyMapTests.cpp)
    target_link_libraries(cs80_keyboard_contour_key_map_tests PRIVATE cs80_core)
    add_test(NAME cs80_keyboard_contour_key_map_tests COMMAND cs80_keyboard_contour_key_map_tests)
    set_tests_properties(cs80_keyboard_contour_key_map_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_touch_circuit_tests tests/TouchCircuitTests.cpp)
    target_link_libraries(cs80_touch_circuit_tests PRIVATE cs80_core)
    add_test(NAME cs80_touch_circuit_tests COMMAND cs80_touch_circuit_tests)
    set_tests_properties(cs80_touch_circuit_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_mpe_master_bend_routing_tests tests/MpeMasterBendRoutingTests.cpp)
    target_link_libraries(cs80_mpe_master_bend_routing_tests PRIVATE cs80_core)
    add_test(NAME cs80_mpe_master_bend_routing_tests COMMAND cs80_mpe_master_bend_routing_tests)
    set_tests_properties(cs80_mpe_master_bend_routing_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_tuning_midi_tests tests/TuningMidiTests.cpp)
    target_link_libraries(cs80_tuning_midi_tests PRIVATE cs80_core)
    target_compile_definitions(cs80_tuning_midi_tests PRIVATE
        CS80_TUNING_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/tuning")
    add_test(NAME cs80_tuning_midi_tests COMMAND cs80_tuning_midi_tests)
    set_tests_properties(cs80_tuning_midi_tests PROPERTIES TIMEOUT 120)

    # Admission-boundary regressions, without a processor or GUI. Preserve
    # the existing Engine-level MTS suite above as a separate contract.
    add_executable(cs80_tuning_frequency_domain_tests tests/TuningFrequencyDomainTests.cpp)
    target_include_directories(cs80_tuning_frequency_domain_tests PRIVATE src)
    add_test(NAME cs80_tuning_frequency_domain_tests COMMAND cs80_tuning_frequency_domain_tests)
    add_executable(cs80_tuning_robustness_tests tests/TuningRobustnessTests.cpp)
    target_include_directories(cs80_tuning_robustness_tests PRIVATE src)
    add_test(NAME cs80_tuning_robustness_tests COMMAND cs80_tuning_robustness_tests)
    set_tests_properties(cs80_tuning_frequency_domain_tests cs80_tuning_robustness_tests PROPERTIES TIMEOUT 30)

    # Build the shipping MTS-ESP adapter in core-only CI as well as plugin builds.
    add_executable(cs80_mts_esp_native_tests tests/MtsEspNativeTests.cpp src/plugin/MtsEspClient.cpp)
    target_include_directories(cs80_mts_esp_native_tests PRIVATE src)
    target_link_libraries(cs80_mts_esp_native_tests PRIVATE ${CMAKE_DL_LIBS})
    add_test(NAME cs80_mts_esp_native_tests COMMAND cs80_mts_esp_native_tests)
    add_executable(cs80_mts_esp_integration_tests tests/MtsEspIntegrationTests.cpp src/plugin/MtsEspClient.cpp)
    target_compile_definitions(cs80_mts_esp_integration_tests PRIVATE
        CS80_WITH_MTS_ESP=1 CS80_MTS_ESP_TEST_BACKEND=1)
    target_link_libraries(cs80_mts_esp_integration_tests PRIVATE cs80_core)
    add_test(NAME cs80_mts_esp_integration_tests COMMAND cs80_mts_esp_integration_tests)
    add_executable(cs80_mts_esp_channel_tests tests/MtsEspChannelTests.cpp src/plugin/MtsEspClient.cpp)
    target_compile_definitions(cs80_mts_esp_channel_tests PRIVATE
        CS80_WITH_MTS_ESP=1 CS80_MTS_ESP_TEST_BACKEND=1)
    target_link_libraries(cs80_mts_esp_channel_tests PRIVATE cs80_core)
    add_test(NAME cs80_mts_esp_channel_tests COMMAND cs80_mts_esp_channel_tests)
    set_tests_properties(cs80_mts_esp_channel_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mts_esp_fallback_tests tests/MtsEspFallbackTests.cpp src/plugin/MtsEspClient.cpp)
    target_include_directories(cs80_mts_esp_fallback_tests PRIVATE src)
    target_compile_definitions(cs80_mts_esp_fallback_tests PRIVATE CS80_WITH_MTS_ESP=0)
    add_test(NAME cs80_mts_esp_fallback_tests COMMAND cs80_mts_esp_fallback_tests)
    add_executable(cs80_mts_esp_windows_macro_tests tests/MtsEspWindowsMacroTests.cpp)
    target_include_directories(cs80_mts_esp_windows_macro_tests PRIVATE src)
    target_compile_definitions(cs80_mts_esp_windows_macro_tests PRIVATE
        CS80_WITH_MTS_ESP=1 CS80_MTS_ESP_TEST_BACKEND=1)
    add_test(NAME cs80_mts_esp_windows_macro_tests COMMAND cs80_mts_esp_windows_macro_tests)
    set_tests_properties(cs80_mts_esp_native_tests cs80_mts_esp_integration_tests
        cs80_mts_esp_fallback_tests cs80_mts_esp_windows_macro_tests PROPERTIES TIMEOUT 120)

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
    add_executable(cs80_modern_lfo_tests tests/ModernLfoTests.cpp)
    target_link_libraries(cs80_modern_lfo_tests PRIVATE cs80_core)
    add_test(NAME cs80_modern_lfo_tests COMMAND cs80_modern_lfo_tests)
    set_tests_properties(cs80_modern_lfo_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mseg_tests tests/MsegTests.cpp)
    target_include_directories(cs80_mseg_tests PRIVATE src)
    add_test(NAME cs80_mseg_tests COMMAND cs80_mseg_tests)
    set_tests_properties(cs80_mseg_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mseg_transport_rate_tests tests/MsegTransportRateTests.cpp)
    target_include_directories(cs80_mseg_transport_rate_tests PRIVATE src)
    add_test(NAME cs80_mseg_transport_rate_tests COMMAND cs80_mseg_transport_rate_tests)
    set_tests_properties(cs80_mseg_transport_rate_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mono_mseg_routing_tests tests/MonoMsegRoutingTests.cpp)
    target_link_libraries(cs80_mono_mseg_routing_tests PRIVATE cs80_core)
    add_test(NAME cs80_mono_mseg_routing_tests COMMAND cs80_mono_mseg_routing_tests)
    set_tests_properties(cs80_mono_mseg_routing_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mseg_retrigger_rate_tests tests/MsegRetriggerRateTests.cpp)
    target_link_libraries(cs80_mseg_retrigger_rate_tests PRIVATE cs80_core)
    add_test(NAME cs80_mseg_retrigger_rate_tests COMMAND cs80_mseg_retrigger_rate_tests)
    set_tests_properties(cs80_mseg_retrigger_rate_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mseg_combined_interaction_tests tests/MsegCombinedInteractionTests.cpp)
    target_link_libraries(cs80_mseg_combined_interaction_tests PRIVATE cs80_core)
    add_test(NAME cs80_mseg_combined_interaction_tests COMMAND cs80_mseg_combined_interaction_tests)
    set_tests_properties(cs80_mseg_combined_interaction_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_mod_matrix_tests tests/ModMatrixTests.cpp)
    target_link_libraries(cs80_mod_matrix_tests PRIVATE cs80_core)
    add_test(NAME cs80_mod_matrix_tests COMMAND cs80_mod_matrix_tests)
    set_tests_properties(cs80_mod_matrix_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_modern_fx_tests tests/ModernFxTests.cpp)
    target_link_libraries(cs80_modern_fx_tests PRIVATE cs80_core)
    add_test(NAME cs80_modern_fx_tests COMMAND cs80_modern_fx_tests)
    set_tests_properties(cs80_modern_fx_tests PROPERTIES TIMEOUT 900)
    add_executable(cs80_delay_line_tests tests/DelayLineTests.cpp)
    target_include_directories(cs80_delay_line_tests PRIVATE src)
    add_test(NAME cs80_delay_line_tests COMMAND cs80_delay_line_tests)
    set_tests_properties(cs80_delay_line_tests PROPERTIES TIMEOUT 60)
    add_executable(cs80_plate_reverb_rate_tests tests/PlateReverbRateTests.cpp)
    target_include_directories(cs80_plate_reverb_rate_tests PRIVATE src)
    add_test(NAME cs80_plate_reverb_rate_tests COMMAND cs80_plate_reverb_rate_tests)
    set_tests_properties(cs80_plate_reverb_rate_tests PROPERTIES TIMEOUT 180)
    add_executable(cs80_tape_echo_time_tests tests/TapeEchoTimeTests.cpp)
    target_include_directories(cs80_tape_echo_time_tests PRIVATE src)
    add_test(NAME cs80_tape_echo_time_tests COMMAND cs80_tape_echo_time_tests)
    set_tests_properties(cs80_tape_echo_time_tests PROPERTIES TIMEOUT 60)
    add_executable(cs80_drive_control_cache_tests tests/DriveControlCacheTests.cpp)
    target_include_directories(cs80_drive_control_cache_tests PRIVATE src)
    add_test(NAME cs80_drive_control_cache_tests COMMAND cs80_drive_control_cache_tests)
    set_tests_properties(cs80_drive_control_cache_tests PROPERTIES TIMEOUT 60)
    add_executable(cs80_ensemble_rate_cache_tests tests/EnsembleRateCacheTests.cpp)
    target_include_directories(cs80_ensemble_rate_cache_tests PRIVATE src)
    add_test(NAME cs80_ensemble_rate_cache_tests COMMAND cs80_ensemble_rate_cache_tests)
    set_tests_properties(cs80_ensemble_rate_cache_tests PROPERTIES TIMEOUT 120)
    add_executable(cs80_tape_echo_control_cache_tests tests/TapeEchoControlCacheTests.cpp)
    target_include_directories(cs80_tape_echo_control_cache_tests PRIVATE src)
    add_test(NAME cs80_tape_echo_control_cache_tests COMMAND cs80_tape_echo_control_cache_tests)
    set_tests_properties(cs80_tape_echo_control_cache_tests PROPERTIES TIMEOUT 180)

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
    add_executable(cs80_memory_panel_tests tests/MemoryPanelTests.cpp)
    target_include_directories(cs80_memory_panel_tests PRIVATE src)
    add_test(NAME cs80_memory_panel_tests
             COMMAND cs80_memory_panel_tests
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/ws6-library.json)

    add_executable(cs80_memory_panel_ports_tests tests/MemoryPanelPortsTests.cpp)
    target_include_directories(cs80_memory_panel_ports_tests PRIVATE src)
    add_test(NAME cs80_memory_panel_ports_tests COMMAND cs80_memory_panel_ports_tests)
    set_tests_properties(cs80_memory_panel_ports_tests PROPERTIES TIMEOUT 120)

    add_executable(cs80_memory_panel_port_map_tests tests/MemoryPanelPortMapTests.cpp)
    target_include_directories(cs80_memory_panel_port_map_tests PRIVATE src)
    add_test(NAME cs80_memory_panel_port_map_tests COMMAND cs80_memory_panel_port_map_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/memory-port-map.json)
    set_tests_properties(cs80_memory_panel_port_map_tests PROPERTIES TIMEOUT 300)
    if (NOT MSVC)
        add_test(NAME cs80_memory_port_map_negative_controls
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memory_port_map_negative_controls.py
                         --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(cs80_memory_port_map_negative_controls PROPERTIES TIMEOUT 300)
    endif()

    add_executable(cs80_memory_control_routing_tests tests/MemoryControlRoutingTests.cpp)
    target_include_directories(cs80_memory_control_routing_tests PRIVATE src)
    add_test(NAME cs80_memory_control_routing_tests COMMAND cs80_memory_control_routing_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/memory-control-routing.json)
    set_tests_properties(cs80_memory_control_routing_tests PROPERTIES TIMEOUT 300)
    add_executable(cs80_memory_receiver_truth_tests tests/MemoryReceiverTruthTests.cpp)
    target_include_directories(cs80_memory_receiver_truth_tests PRIVATE src)
    add_test(NAME cs80_memory_receiver_truth_tests COMMAND cs80_memory_receiver_truth_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/memory-nonlinear-study.json)
    set_tests_properties(cs80_memory_receiver_truth_tests PROPERTIES TIMEOUT 300)
    # Opt-in research probe: reports failures of candidate approximation budgets,
    # not a shipping-audio test or a whole-instrument performance benchmark.
    add_executable(cs80_memory_qualification_probe tests/MemoryQualificationProbe.cpp)
    target_include_directories(cs80_memory_qualification_probe PRIVATE src)
    add_test(NAME cs80_memory_qualification_probe_self_tests COMMAND cs80_memory_qualification_probe
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/memory-nonlinear-study.json
             ${CMAKE_CURRENT_BINARY_DIR} check)
    set_tests_properties(cs80_memory_qualification_probe_self_tests PROPERTIES TIMEOUT 30)
    add_executable(cs80_memory_verified_diode_tests tests/MemoryVerifiedDiodeTests.cpp)
    target_include_directories(cs80_memory_verified_diode_tests PRIVATE src)
    add_test(NAME cs80_memory_verified_diode_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memory_verified_diode.py
                     $<TARGET_FILE:cs80_memory_verified_diode_tests>)
    set_tests_properties(cs80_memory_verified_diode_tests PROPERTIES TIMEOUT 300)
    add_test(NAME cs80_memory_centered_log_rational
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memory_centered_log_rational.py
                     $<TARGET_FILE:cs80_memory_verified_diode_tests>)
    set_tests_properties(cs80_memory_centered_log_rational PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_memory_interval_extrema
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memory_interval_extrema.py
                     $<TARGET_FILE:cs80_memory_verified_diode_tests>)
    set_tests_properties(cs80_memory_interval_extrema PROPERTIES TIMEOUT 60)
    add_executable(cs80_memory_sparse_update_tests tests/MemorySparseUpdateTests.cpp)
    target_include_directories(cs80_memory_sparse_update_tests PRIVATE src)
    add_test(NAME cs80_memory_sparse_update_tests COMMAND cs80_memory_sparse_update_tests
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/memory-sparse-update.json)
    set_tests_properties(cs80_memory_sparse_update_tests PROPERTIES TIMEOUT 300)
    foreach(memory_target cs80_memory_verified_diode_tests cs80_memory_sparse_update_tests)
        if (MSVC)
            target_compile_options(${memory_target} PRIVATE /fp:strict)
        else()
            target_compile_options(${memory_target} PRIVATE -ffp-contract=off)
        endif()
    endforeach()
    if (NOT MSVC)
        add_executable(cs80_memory_sparse_probe tests/MemorySparseProbe.cpp)
        target_include_directories(cs80_memory_sparse_probe PRIVATE src)
        target_compile_options(cs80_memory_sparse_probe PRIVATE -ffp-contract=off)
        add_test(NAME cs80_memory_sparse_negative_controls
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memory_sparse_negative_controls.py
                         --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(cs80_memory_sparse_negative_controls PROPERTIES TIMEOUT 300)
    endif()
    if (NOT MSVC)
        add_test(NAME cs80_memory_routing_negative_controls
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_memory_routing_negative_controls.py
                         --compiler ${CMAKE_CXX_COMPILER})
        set_tests_properties(cs80_memory_routing_negative_controls PROPERTIES TIMEOUT 300)
    endif()

    add_executable(cs80_programmer_control_identity_tests tests/ProgrammerControlIdentityTests.cpp)
    target_include_directories(cs80_programmer_control_identity_tests PRIVATE src)
    add_test(NAME cs80_programmer_control_identity_tests COMMAND cs80_programmer_control_identity_tests)

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
if (CS80_BUILD_TESTS)
    add_test(NAME cs80_render_cli_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_cli.py
                     --render $<TARGET_FILE:cs80_render>)
    set_tests_properties(cs80_render_cli_tests PROPERTIES TIMEOUT 60)
endif()

add_executable(cs80_source_factory_audition src/tools/source_factory_audition.cpp)
target_link_libraries(cs80_source_factory_audition PRIVATE cs80_core)

add_executable(cs80_voice_continuity_probe src/tools/voice_continuity_probe.cpp)
target_include_directories(cs80_voice_continuity_probe PRIVATE src)

add_executable(cs80_source_factory_benchmark src/tools/source_factory_benchmark.cpp)
target_link_libraries(cs80_source_factory_benchmark PRIVATE cs80_core)
if (CS80_BUILD_TESTS)
    add_test(NAME cs80_benchmark_preflight_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_benchmark_preflight.py
                     --benchmark $<TARGET_FILE:cs80_source_factory_benchmark>)
    set_tests_properties(cs80_benchmark_preflight_tests PROPERTIES TIMEOUT 60)
endif()

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
    include(cmake/PresetXml.cmake)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/juce/CMakeLists.txt")
        add_subdirectory(deps/juce)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                                  # Official 8.0.11: fixes Linux locale-query restoration.
                                  GIT_TAG c352e2489000ffeb9e397051377d75eef2efe641)
        FetchContent_MakeAvailable(JUCE)
    endif()

    if (CS80_BUILD_TESTS)
        # Production modern-state number parser only: no processor or GUI initialization.
        juce_add_console_app(cs80_modern_state_integer_tests
            PRODUCT_NAME "CS80 Modern State Integer Tests")
        target_sources(cs80_modern_state_integer_tests PRIVATE tests/ModernStateIntegerTests.cpp)
        target_include_directories(cs80_modern_state_integer_tests PRIVATE src)
        target_link_libraries(cs80_modern_state_integer_tests PRIVATE juce::juce_core)
        target_compile_definitions(cs80_modern_state_integer_tests PRIVATE JUCE_USE_CURL=0)
        add_test(NAME cs80_modern_state_integer_tests COMMAND cs80_modern_state_integer_tests)
        set_tests_properties(cs80_modern_state_integer_tests PROPERTIES TIMEOUT 30)

        # XML display metadata only: no processor or GUI initialization.
        juce_add_console_app(cs80_preset_display_name_tests
            PRODUCT_NAME "CS80 Preset Display Name Tests")
        target_sources(cs80_preset_display_name_tests PRIVATE tests/PresetDisplayNameTests.cpp)
        target_include_directories(cs80_preset_display_name_tests PRIVATE src)
        target_link_libraries(cs80_preset_display_name_tests PRIVATE juce::juce_core)
        target_compile_definitions(cs80_preset_display_name_tests PRIVATE JUCE_USE_CURL=0)
        add_test(NAME cs80_preset_display_name_tests COMMAND cs80_preset_display_name_tests)
        set_tests_properties(cs80_preset_display_name_tests PROPERTIES TIMEOUT 30)

        # Preset browser XML values only: no processor or GUI initialization.
        juce_add_console_app(cs80_preset_file_values_tests
            PRODUCT_NAME "CS80 Preset File Values Tests")
        target_sources(cs80_preset_file_values_tests PRIVATE tests/PresetFileValuesTests.cpp)
        target_include_directories(cs80_preset_file_values_tests PRIVATE src)
        target_link_libraries(cs80_preset_file_values_tests PRIVATE juce::juce_core)
        target_compile_definitions(cs80_preset_file_values_tests PRIVATE JUCE_USE_CURL=0)
        add_test(NAME cs80_preset_file_values_tests COMMAND cs80_preset_file_values_tests)
        set_tests_properties(cs80_preset_file_values_tests PROPERTIES TIMEOUT 30)

        # MSEG text/XML codec only: no processor or GUI initialization.
        juce_add_console_app(cs80_mseg_state_text_tests
            PRODUCT_NAME "CS80 MSEG State Text Tests")
        target_sources(cs80_mseg_state_text_tests PRIVATE tests/MsegStateTextTests.cpp)
        target_include_directories(cs80_mseg_state_text_tests PRIVATE src)
        target_link_libraries(cs80_mseg_state_text_tests PRIVATE juce::juce_core)
        target_compile_definitions(cs80_mseg_state_text_tests PRIVATE JUCE_USE_CURL=0)
        add_test(NAME cs80_mseg_state_text_tests COMMAND cs80_mseg_state_text_tests)
        set_tests_properties(cs80_mseg_state_text_tests PROPERTIES TIMEOUT 30)

        # Detached ValueTree matching only: no processor or GUI initialization.
        juce_add_console_app(cs80_parameter_state_index_tests
            PRODUCT_NAME "CS80 Parameter State Index Tests")
        target_sources(cs80_parameter_state_index_tests PRIVATE tests/ParameterStateChildIndexTests.cpp)
        target_include_directories(cs80_parameter_state_index_tests PRIVATE src)
        target_link_libraries(cs80_parameter_state_index_tests PRIVATE juce::juce_data_structures)
        target_compile_definitions(cs80_parameter_state_index_tests PRIVATE JUCE_USE_CURL=0)
        add_test(NAME cs80_parameter_state_index_tests COMMAND cs80_parameter_state_index_tests)
        set_tests_properties(cs80_parameter_state_index_tests PROPERTIES TIMEOUT 120)
    endif()

    # This main-based feature is VST3-only. Keep the shipping AU wrapper intact.
    set(CS80_HOST_AUTOMATION_FORMATS VST3)
    include(cmake/HostAutomation.cmake)
    include(cmake/ParameterStateMirror.cmake)
    if(CS80_BUILD_TESTS)
        add_test(NAME cs80_state_mirror_bridge_tests COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/StateMirrorBridgeTests.py" "${_cs80_module_dir}")
        set_tests_properties(cs80_state_mirror_bridge_tests PROPERTIES TIMEOUT 30)
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
        src/plugin/PluginProcessorAutomation.cpp
        src/plugin/PluginEditor.cpp
        src/plugin/UiComponents.cpp
        src/plugin/AdvancedPanel.cpp
        src/plugin/ParameterComboBox.cpp
        src/plugin/ParameterContextButton.cpp
        src/plugin/PresetBrowser.cpp

        # [ws1-voice] add this workstream's plugin sources below this line.

        # [ws2-engine] add this workstream's plugin sources below this line.

        # [ws3-keyboard] add this workstream's plugin sources below this line.
        src/plugin/MtsEspClient.cpp
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
        LibXml2::LibXml2
        cs80_core
        ${CMAKE_DL_LIBS}
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
        add_executable(cs80_vst3_ring_automation_tests tests/Vst3RingAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_ring_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_ring_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_ring_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(ring_test_name "cs80_vst3_ring_automation_${rate}_${mode}_tests")
                add_test(NAME "${ring_test_name}" COMMAND cs80_vst3_ring_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-ring-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/ring-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${ring_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_ring_depth_automation_tests tests/Vst3RingDepthAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_ring_depth_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_ring_depth_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_ring_depth_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(ring_depth_test_name "cs80_vst3_ring_depth_automation_${rate}_${mode}_tests")
                add_test(NAME "${ring_depth_test_name}" COMMAND cs80_vst3_ring_depth_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-ring-depth-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/ring-depth-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${ring_depth_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_ring_times_automation_tests tests/Vst3RingTimesAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_ring_times_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_ring_times_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_ring_times_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(ring_times_test_name "cs80_vst3_ring_times_automation_${rate}_${mode}_tests")
                add_test(NAME "${ring_times_test_name}" COMMAND cs80_vst3_ring_times_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-ring-times-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/ring-times-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${ring_times_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_ring_retrigger_automation_tests tests/Vst3RingRetriggerAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_ring_retrigger_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_ring_retrigger_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_ring_retrigger_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(ring_retrigger_test_name "cs80_vst3_ring_retrigger_automation_${rate}_${mode}_tests")
                add_test(NAME "${ring_retrigger_test_name}" COMMAND cs80_vst3_ring_retrigger_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-ring-retrigger-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/ring-retrigger-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${ring_retrigger_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_ring_combined_automation_tests tests/Vst3RingCombinedAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_ring_combined_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_ring_combined_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_ring_combined_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(ring_combined_test_name "cs80_vst3_ring_combined_automation_${rate}_${mode}_tests")
                add_test(NAME "${ring_combined_test_name}" COMMAND cs80_vst3_ring_combined_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-ring-combined-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/ring-combined-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${ring_combined_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_chorus_automation_tests tests/Vst3ChorusAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_chorus_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_chorus_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_chorus_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(chorus_test_name "cs80_vst3_chorus_automation_${rate}_${mode}_tests")
                add_test(NAME "${chorus_test_name}" COMMAND cs80_vst3_chorus_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-chorus-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/chorus-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${chorus_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_tremolo_automation_tests tests/Vst3TremoloAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_tremolo_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_tremolo_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_tremolo_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(tremolo_test_name "cs80_vst3_tremolo_automation_${rate}_${mode}_tests")
                add_test(NAME "${tremolo_test_name}" COMMAND cs80_vst3_tremolo_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-tremolo-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/tremolo-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${tremolo_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_portamento_automation_tests tests/Vst3PortamentoAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_portamento_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_portamento_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_portamento_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(portamento_test_name "cs80_vst3_portamento_automation_${rate}_${mode}_tests")
                add_test(NAME "${portamento_test_name}" COMMAND cs80_vst3_portamento_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-portamento-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/portamento-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${portamento_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_master_sustain_automation_tests tests/Vst3MasterSustainAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_master_sustain_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_master_sustain_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_master_sustain_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(master_sustain_test_name "cs80_vst3_master_sustain_automation_${rate}_${mode}_tests")
                add_test(NAME "${master_sustain_test_name}" COMMAND cs80_vst3_master_sustain_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-master-sustain-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/master-sustain-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${master_sustain_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_initial_bend_automation_tests tests/Vst3InitialBendAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_initial_bend_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_initial_bend_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_initial_bend_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(initial_bend_test_name "cs80_vst3_initial_bend_automation_${rate}_${mode}_tests")
                add_test(NAME "${initial_bend_test_name}" COMMAND cs80_vst3_initial_bend_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-initial-bend-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/initial-bend-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${initial_bend_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_kbd_level_low_automation_tests tests/Vst3KbdLevelLowAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_kbd_level_low_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_kbd_level_low_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_kbd_level_low_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(kbd_level_low_test_name "cs80_vst3_kbd_level_low_automation_${rate}_${mode}_tests")
                add_test(NAME "${kbd_level_low_test_name}" COMMAND cs80_vst3_kbd_level_low_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-kbd-level-low-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/kbd-level-low-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${kbd_level_low_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_kbd_level_high_automation_tests tests/Vst3KbdLevelHighAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_kbd_level_high_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_kbd_level_high_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_kbd_level_high_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(kbd_level_high_test_name "cs80_vst3_kbd_level_high_automation_${rate}_${mode}_tests")
                add_test(NAME "${kbd_level_high_test_name}" COMMAND cs80_vst3_kbd_level_high_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-kbd-level-high-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/kbd-level-high-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${kbd_level_high_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_kbd_brilliance_automation_tests tests/Vst3KbdBrillianceAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_kbd_brilliance_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_kbd_brilliance_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_kbd_brilliance_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(kbd_brilliance_test_name "cs80_vst3_kbd_brilliance_automation_${rate}_${mode}_tests")
                add_test(NAME "${kbd_brilliance_test_name}" COMMAND cs80_vst3_kbd_brilliance_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-kbd-brilliance-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/kbd-brilliance-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${kbd_brilliance_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_initial_touch_automation_tests tests/Vst3InitialTouchAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_initial_touch_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_initial_touch_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_initial_touch_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(initial_touch_test_name "cs80_vst3_initial_touch_automation_${rate}_${mode}_tests")
                add_test(NAME "${initial_touch_test_name}" COMMAND cs80_vst3_initial_touch_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-initial-touch-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/initial-touch-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${initial_touch_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_after_touch_automation_tests tests/Vst3AfterTouchAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_after_touch_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_after_touch_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_after_touch_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(after_touch_test_name "cs80_vst3_after_touch_automation_${rate}_${mode}_tests")
                add_test(NAME "${after_touch_test_name}" COMMAND cs80_vst3_after_touch_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-after-touch-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/after-touch-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${after_touch_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_pwm_automation_tests tests/Vst3PwmAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_pwm_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_pwm_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_pwm_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(pwm_test_name "cs80_vst3_pwm_automation_${rate}_${mode}_tests")
                add_test(NAME "${pwm_test_name}" COMMAND cs80_vst3_pwm_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-pwm-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/pwm-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${pwm_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_vcf_level_automation_tests tests/Vst3VcfLevelAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_vcf_level_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_vcf_level_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_vcf_level_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(vcf_level_test_name "cs80_vst3_vcf_level_automation_${rate}_${mode}_tests")
                add_test(NAME "${vcf_level_test_name}" COMMAND cs80_vst3_vcf_level_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-vcf-level-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/vcf-level-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${vcf_level_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_vca_sustain_automation_tests tests/Vst3VcaSustainAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_vca_sustain_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_vca_sustain_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_vca_sustain_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(vca_sustain_test_name "cs80_vst3_vca_sustain_automation_${rate}_${mode}_tests")
                add_test(NAME "${vca_sustain_test_name}" COMMAND cs80_vst3_vca_sustain_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-vca-sustain-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/vca-sustain-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${vca_sustain_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_filter_il_automation_tests tests/Vst3FilterIlAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_filter_il_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_filter_il_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_filter_il_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(filter_il_test_name "cs80_vst3_filter_il_automation_${rate}_${mode}_tests")
                add_test(NAME "${filter_il_test_name}" COMMAND cs80_vst3_filter_il_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-filter-il-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/filter-il-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${filter_il_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_filter_al_automation_tests tests/Vst3FilterAlAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_filter_al_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_filter_al_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_filter_al_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(filter_al_test_name "cs80_vst3_filter_al_automation_${rate}_${mode}_tests")
                add_test(NAME "${filter_al_test_name}" COMMAND cs80_vst3_filter_al_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-filter-al-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/filter-al-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${filter_al_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_envelope_attack_automation_tests tests/Vst3EnvelopeAttackAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_envelope_attack_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_envelope_attack_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_envelope_attack_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(envelope_attack_test_name "cs80_vst3_envelope_attack_automation_${rate}_${mode}_tests")
                add_test(NAME "${envelope_attack_test_name}" COMMAND cs80_vst3_envelope_attack_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-envelope-attack-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/envelope-attack-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${envelope_attack_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_envelope_decay_automation_tests tests/Vst3EnvelopeDecayAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_envelope_decay_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_envelope_decay_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_envelope_decay_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(envelope_decay_test_name "cs80_vst3_envelope_decay_automation_${rate}_${mode}_tests")
                add_test(NAME "${envelope_decay_test_name}" COMMAND cs80_vst3_envelope_decay_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-envelope-decay-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/envelope-decay-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${envelope_decay_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_envelope_release_automation_tests tests/Vst3EnvelopeReleaseAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_envelope_release_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_envelope_release_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_envelope_release_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(envelope_release_test_name "cs80_vst3_envelope_release_automation_${rate}_${mode}_tests")
                add_test(NAME "${envelope_release_test_name}" COMMAND cs80_vst3_envelope_release_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-envelope-release-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/envelope-release-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${envelope_release_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_kbd_contour_combined_automation_tests tests/Vst3KbdContourCombinedAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_kbd_contour_combined_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_kbd_contour_combined_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_kbd_contour_combined_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(kbd_contour_combined_test_name "cs80_vst3_kbd_contour_combined_automation_${rate}_${mode}_tests")
                add_test(NAME "${kbd_contour_combined_test_name}" COMMAND cs80_vst3_kbd_contour_combined_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-kbd-contour-combined-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/kbd-contour-combined-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${kbd_contour_combined_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_glissando_automation_tests tests/Vst3GlissandoAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_glissando_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_glissando_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_glissando_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(glissando_test_name "cs80_vst3_glissando_automation_${rate}_${mode}_tests")
                add_test(NAME "${glissando_test_name}" COMMAND cs80_vst3_glissando_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-glissando-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/glissando-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${glissando_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_ring_chorus_combined_automation_tests tests/Vst3RingChorusCombinedAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_ring_chorus_combined_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_ring_chorus_combined_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_ring_chorus_combined_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(ring_chorus_combined_test_name "cs80_vst3_ring_chorus_combined_automation_${rate}_${mode}_tests")
                add_test(NAME "${ring_chorus_combined_test_name}" COMMAND cs80_vst3_ring_chorus_combined_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-ring-chorus-combined-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/ring-chorus-combined-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${ring_chorus_combined_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_legacy_speed_automation_tests tests/Vst3SubLegacySpeedAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_legacy_speed_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_sub_legacy_speed_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_legacy_speed_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(legacy_speed_test_name "cs80_vst3_sub_legacy_speed_automation_${rate}_${mode}_tests")
                add_test(NAME "${legacy_speed_test_name}" COMMAND cs80_vst3_sub_legacy_speed_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-legacy-speed-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/sub-legacy-speed-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${legacy_speed_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_touch_automation_tests tests/Vst3SubTouchAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_touch_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_sub_touch_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_touch_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(touch_test_name "cs80_vst3_sub_touch_automation_${rate}_${mode}_tests")
                add_test(NAME "${touch_test_name}" COMMAND cs80_vst3_sub_touch_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-touch-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/sub-touch-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${touch_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_touch_combined_automation_tests tests/Vst3SubTouchCombinedAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_touch_combined_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_sub_touch_combined_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_touch_combined_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(touch_combined_test_name "cs80_vst3_sub_touch_combined_automation_${rate}_${mode}_tests")
                add_test(NAME "${touch_combined_test_name}" COMMAND cs80_vst3_sub_touch_combined_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-touch-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/sub-touch-combined-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${touch_combined_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_legacy_combined_automation_tests tests/Vst3SubLegacyCombinedAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_legacy_combined_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_sub_legacy_combined_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_legacy_combined_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(legacy_combined_test_name "cs80_vst3_sub_legacy_combined_automation_${rate}_${mode}_tests")
                add_test(NAME "${legacy_combined_test_name}" COMMAND cs80_vst3_sub_legacy_combined_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-legacy-speed-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/sub-legacy-combined-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${legacy_combined_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_touch_legacy_automation_tests tests/Vst3SubTouchAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_touch_legacy_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_compile_definitions(cs80_vst3_sub_touch_legacy_automation_tests PRIVATE CS80_SUB_TOUCH_LEGACY_OSCILLATOR=1)
        target_link_libraries(cs80_vst3_sub_touch_legacy_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_touch_legacy_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(touch_legacy_test_name "cs80_vst3_sub_touch_legacy_automation_${rate}_${mode}_tests")
                add_test(NAME "${touch_legacy_test_name}" COMMAND cs80_vst3_sub_touch_legacy_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-touch-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/sub-touch-legacy-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${touch_legacy_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_waveform_automation_tests tests/Vst3SubWaveformAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_waveform_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_sub_waveform_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_waveform_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(waveform_test_name "cs80_vst3_sub_waveform_automation_${rate}_${mode}_tests")
                add_test(NAME "${waveform_test_name}" COMMAND cs80_vst3_sub_waveform_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${rate}" "${mode}")
                set_tests_properties("${waveform_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_depth_automation_tests tests/Vst3SubAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_depth_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_compile_definitions(cs80_vst3_sub_depth_automation_tests PRIVATE CS80_SUB_DEPTH_EXTENSION=1)
        target_link_libraries(cs80_vst3_sub_depth_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_depth_automation_tests CS80Reference_VST3)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                set(depth_test_name "cs80_vst3_sub_depth_automation_${rate}_${mode}_tests")
                add_test(NAME "${depth_test_name}" COMMAND cs80_vst3_sub_depth_automation_tests
                    "$<TARGET_FILE:CS80Reference_VST3>"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-depth-v1.json"
                    "${CMAKE_CURRENT_BINARY_DIR}/sub-depth-automation-capture-${rate}-${mode}"
                    "${rate}" "${mode}")
                set_tests_properties("${depth_test_name}" PROPERTIES TIMEOUT 180)
            endforeach()
        endforeach()
        add_executable(cs80_vst3_sub_automation_tests tests/Vst3SubAutomationTests.cpp
            "${CS80_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
            "${CS80_VST3_SDK}/pluginterfaces/base/funknown.cpp"
            "${CS80_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
        target_include_directories(cs80_vst3_sub_automation_tests PRIVATE src "${CS80_VST3_SDK}")
        target_link_libraries(cs80_vst3_sub_automation_tests PRIVATE ${CMAKE_DL_LIBS})
        add_dependencies(cs80_vst3_sub_automation_tests CS80Reference_VST3)
        add_test(NAME cs80_vst3_sub_automation_tests COMMAND cs80_vst3_sub_automation_tests
            "$<TARGET_FILE:CS80Reference_VST3>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-v1.json"
            "${CMAKE_CURRENT_BINARY_DIR}/sub-automation-capture")
        set_tests_properties(cs80_vst3_sub_automation_tests PROPERTIES TIMEOUT 180)
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                if(NOT (rate STREQUAL "48000" AND mode STREQUAL "realtime"))
                    set(sub_test_name "cs80_vst3_sub_automation_${rate}_${mode}_tests")
                    add_test(NAME "${sub_test_name}" COMMAND cs80_vst3_sub_automation_tests
                        "$<TARGET_FILE:CS80Reference_VST3>"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation-sub-v1.json"
                        "${CMAKE_CURRENT_BINARY_DIR}/sub-automation-capture-${rate}-${mode}"
                        "${rate}" "${mode}")
                    set_tests_properties("${sub_test_name}" PROPERTIES TIMEOUT 180)
                endif()
            endforeach()
        endforeach()
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
        foreach(rate IN ITEMS 44100 48000 96000)
            foreach(mode IN ITEMS realtime offline)
                if(NOT (rate STREQUAL "48000" AND mode STREQUAL "realtime"))
                    set(test_name "cs80_vst3_automation_${rate}_${mode}_tests")
                    add_test(NAME "${test_name}" COMMAND cs80_vst3_automation_tests
                        "$<TARGET_FILE:CS80Reference_VST3>"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/host-automation.json"
                        "${rate}" "${mode}")
                    set_tests_properties("${test_name}" PROPERTIES TIMEOUT 180)
                endif()
            endforeach()
        endforeach()
        # Only this executable substitutes the external MTS C API. The shipping
        # static library's client object is not pulled when these symbols resolve.
        add_executable(cs80_mts_esp_processor_tests tests/MtsEspProcessorTests.cpp src/plugin/MtsEspClient.cpp)
        target_include_directories(cs80_mts_esp_processor_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mts_esp_processor_tests PRIVATE CS80_MTS_ESP_TEST_BACKEND=1
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mts_esp_processor_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mts_esp_processor_tests COMMAND cs80_mts_esp_processor_tests)
        set_tests_properties(cs80_mts_esp_processor_tests PROPERTIES TIMEOUT 120)
        # Explicit opt-in, namespace-dependent qualification. Never a default build/CTest.
        # Only the copied official client changes its library loader path; shipping
        # CS80Reference and its production MtsEspClient.cpp remain unchanged.
        set(CS80_MTS_REAL_CLIENT_SOURCE "" CACHE FILEPATH "Isolated generated official libMTSClient.cpp")
        set(CS80_MTS_REAL_BUILD_RPATH "" CACHE STRING "Optional verified dependency RUNPATH for real MTS processor qualification")
        if(CS80_MTS_REAL_CLIENT_SOURCE)
            if(NOT IS_ABSOLUTE "${CS80_MTS_REAL_CLIENT_SOURCE}" OR NOT EXISTS "${CS80_MTS_REAL_CLIENT_SOURCE}")
                message(FATAL_ERROR "CS80_MTS_REAL_CLIENT_SOURCE must be an existing absolute generated client path")
            endif()
            add_executable(cs80_mts_real_processor EXCLUDE_FROM_ALL
                tests/mts_real_library/processor.cpp src/plugin/MtsEspClient.cpp "${CS80_MTS_REAL_CLIENT_SOURCE}")
            target_include_directories(cs80_mts_real_processor PRIVATE src
                "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
            target_compile_definitions(cs80_mts_real_processor PRIVATE CS80_MTS_ESP_TEST_BACKEND=1
                "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
            target_link_libraries(cs80_mts_real_processor PRIVATE CS80Reference ${CMAKE_DL_LIBS})
            set_target_properties(cs80_mts_real_processor PROPERTIES BUILD_RPATH "${CS80_MTS_REAL_BUILD_RPATH}")
        endif()
        add_executable(cs80_sub_routing_display_tests tests/SubRoutingDisplayTests.cpp)
        target_include_directories(cs80_sub_routing_display_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_sub_routing_display_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_sub_routing_display_tests PRIVATE CS80Reference)
        add_test(NAME cs80_sub_routing_display_tests COMMAND cs80_sub_routing_display_tests)
        set_tests_properties(cs80_sub_routing_display_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_parameter_admission_tests tests/ParameterValueAdmissionTests.cpp)
        target_include_directories(cs80_parameter_admission_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_parameter_admission_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_parameter_admission_tests PRIVATE CS80Reference)
        add_test(NAME cs80_parameter_admission_tests COMMAND cs80_parameter_admission_tests)
        set_tests_properties(cs80_parameter_admission_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_manager_range_retarget_host_tests tests/MpeManagerRangeRetargetHostTests.cpp)
        target_include_directories(cs80_mpe_manager_range_retarget_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_manager_range_retarget_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_manager_range_retarget_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_manager_range_retarget_host_tests COMMAND cs80_mpe_manager_range_retarget_host_tests)
        set_tests_properties(cs80_mpe_manager_range_retarget_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_member_range_retarget_host_tests tests/MpeMemberRangeRetargetHostTests.cpp)
        target_include_directories(cs80_mpe_member_range_retarget_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_member_range_retarget_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_member_range_retarget_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_member_range_retarget_host_tests COMMAND cs80_mpe_member_range_retarget_host_tests)
        set_tests_properties(cs80_mpe_member_range_retarget_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_membership_reset_host_tests tests/MpeMembershipResetHostTests.cpp)
        target_include_directories(cs80_mpe_membership_reset_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_membership_reset_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_membership_reset_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_membership_reset_host_tests COMMAND cs80_mpe_membership_reset_host_tests)
        set_tests_properties(cs80_mpe_membership_reset_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_role_transition_host_tests tests/MpeRoleTransitionHostTests.cpp)
        target_include_directories(cs80_mpe_role_transition_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_role_transition_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_role_transition_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_role_transition_host_tests COMMAND cs80_mpe_role_transition_host_tests)
        set_tests_properties(cs80_mpe_role_transition_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_program_arp_handoff_host_tests tests/ProgramArpHandoffHostTests.cpp)
        target_include_directories(cs80_program_arp_handoff_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_program_arp_handoff_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_program_arp_handoff_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_program_arp_handoff_host_tests COMMAND cs80_program_arp_handoff_host_tests)
        set_tests_properties(cs80_program_arp_handoff_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_timbre_combination_host_tests tests/MpeTimbreCombinationHostTests.cpp)
        target_include_directories(cs80_mpe_timbre_combination_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_timbre_combination_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_timbre_combination_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_timbre_combination_host_tests COMMAND cs80_mpe_timbre_combination_host_tests)
        set_tests_properties(cs80_mpe_timbre_combination_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_pressure_combination_host_tests tests/MpePressureCombinationHostTests.cpp)
        target_include_directories(cs80_mpe_pressure_combination_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_pressure_combination_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_pressure_combination_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_pressure_combination_host_tests COMMAND cs80_mpe_pressure_combination_host_tests)
        set_tests_properties(cs80_mpe_pressure_combination_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_poly_pressure_host_tests tests/MpePolyPressureHostTests.cpp)
        target_include_directories(cs80_mpe_poly_pressure_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_poly_pressure_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_poly_pressure_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_poly_pressure_host_tests COMMAND cs80_mpe_poly_pressure_host_tests)
        set_tests_properties(cs80_mpe_poly_pressure_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_program_routing_host_tests tests/MpeProgramRoutingHostTests.cpp)
        target_include_directories(cs80_mpe_program_routing_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_program_routing_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_program_routing_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_program_routing_host_tests COMMAND cs80_mpe_program_routing_host_tests)
        set_tests_properties(cs80_mpe_program_routing_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_configuration_range_host_tests tests/MpeConfigurationRangeHostTests.cpp)
        target_include_directories(cs80_mpe_configuration_range_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_configuration_range_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_configuration_range_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_configuration_range_host_tests COMMAND cs80_mpe_configuration_range_host_tests)
        set_tests_properties(cs80_mpe_configuration_range_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_controller_handoff_host_tests tests/ControllerHandoffHostTests.cpp)
        target_include_directories(cs80_controller_handoff_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_controller_handoff_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_controller_handoff_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_controller_handoff_host_tests COMMAND cs80_controller_handoff_host_tests)
        set_tests_properties(cs80_controller_handoff_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_modern_state_notification_tests tests/ModernStateNotificationTests.cpp)
        target_include_directories(cs80_modern_state_notification_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_modern_state_notification_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_modern_state_notification_tests PRIVATE CS80Reference)
        add_test(NAME cs80_modern_state_notification_tests COMMAND cs80_modern_state_notification_tests)
        set_tests_properties(cs80_modern_state_notification_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_tuning_state_notification_tests tests/TuningStateNotificationTests.cpp)
        target_include_directories(cs80_tuning_state_notification_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_tuning_state_notification_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_tuning_state_notification_tests PRIVATE CS80Reference)
        add_test(NAME cs80_tuning_state_notification_tests COMMAND cs80_tuning_state_notification_tests)
        set_tests_properties(cs80_tuning_state_notification_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_midi_map_change_tests tests/MidiMapChangeTests.cpp)
        target_include_directories(cs80_midi_map_change_tests PRIVATE src/plugin)
        add_test(NAME cs80_midi_map_change_tests COMMAND cs80_midi_map_change_tests)
        add_executable(cs80_ab_copy_notification_tests tests/AbCopyNotificationTests.cpp)
        target_include_directories(cs80_ab_copy_notification_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_ab_copy_notification_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_ab_copy_notification_tests PRIVATE CS80Reference)
        add_test(NAME cs80_ab_copy_notification_tests COMMAND cs80_ab_copy_notification_tests)
        set_tests_properties(cs80_ab_copy_notification_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_studio_preference_notification_tests tests/StudioPreferenceNotificationTests.cpp)
        target_include_directories(cs80_studio_preference_notification_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_studio_preference_notification_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_studio_preference_notification_tests PRIVATE CS80Reference)
        add_test(NAME cs80_studio_preference_notification_tests COMMAND cs80_studio_preference_notification_tests)
        set_tests_properties(cs80_studio_preference_notification_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_studio_state_sync_tests tests/StudioStateSyncTests.cpp)
        target_include_directories(cs80_studio_state_sync_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_studio_state_sync_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_studio_state_sync_tests PRIVATE CS80Reference)
        add_test(NAME cs80_studio_state_sync_tests COMMAND cs80_studio_state_sync_tests)
        set_tests_properties(cs80_studio_state_sync_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_midi_map_notification_tests tests/MidiMapNotificationTests.cpp)
        target_include_directories(cs80_midi_map_notification_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_midi_map_notification_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_midi_map_notification_tests PRIVATE CS80Reference)
        add_test(NAME cs80_midi_map_notification_tests COMMAND cs80_midi_map_notification_tests)
        set_tests_properties(cs80_midi_map_notification_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_voice_monitor_accessibility_tests tests/VoiceMonitorAccessibilityTests.cpp)
        target_include_directories(cs80_voice_monitor_accessibility_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_voice_monitor_accessibility_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_voice_monitor_accessibility_tests PRIVATE CS80Reference)
        add_test(NAME cs80_voice_monitor_accessibility_tests COMMAND cs80_voice_monitor_accessibility_tests)
        set_tests_properties(cs80_voice_monitor_accessibility_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_render_policy_context_tests tests/RenderPolicyContextTests.cpp)
        target_include_directories(cs80_render_policy_context_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_render_policy_context_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_render_policy_context_tests PRIVATE CS80Reference)
        add_test(NAME cs80_render_policy_context_tests COMMAND cs80_render_policy_context_tests)
        set_tests_properties(cs80_render_policy_context_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_voice_monitor_focus_tests tests/VoiceMonitorFocusTests.cpp)
        target_include_directories(cs80_voice_monitor_focus_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_voice_monitor_focus_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_voice_monitor_focus_tests PRIVATE CS80Reference)
        add_test(NAME cs80_voice_monitor_focus_tests COMMAND cs80_voice_monitor_focus_tests)
        set_tests_properties(cs80_voice_monitor_focus_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_voice_inspection_host_tests tests/VoiceInspectionHostTests.cpp)
        target_include_directories(cs80_voice_inspection_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_voice_inspection_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_voice_inspection_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_voice_inspection_host_tests COMMAND cs80_voice_inspection_host_tests)
        set_tests_properties(cs80_voice_inspection_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_tuning_inspection_host_tests tests/TuningInspectionHostTests.cpp)
        target_include_directories(cs80_tuning_inspection_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_tuning_inspection_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_tuning_inspection_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_tuning_inspection_host_tests COMMAND cs80_tuning_inspection_host_tests)
        set_tests_properties(cs80_tuning_inspection_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_mpe_status_host_tests tests/MpeStatusHostTests.cpp)
        target_include_directories(cs80_mpe_status_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_mpe_status_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_mpe_status_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_mpe_status_host_tests COMMAND cs80_mpe_status_host_tests)
        set_tests_properties(cs80_mpe_status_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_program_extension_recall_host_tests tests/ProgramExtensionRecallHostTests.cpp)
        target_include_directories(cs80_program_extension_recall_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_program_extension_recall_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_program_extension_recall_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_program_extension_recall_host_tests COMMAND cs80_program_extension_recall_host_tests)
        set_tests_properties(cs80_program_extension_recall_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_state_mirror_host_tests tests/StateMirrorHostTests.cpp)
        target_include_directories(cs80_state_mirror_host_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_state_mirror_host_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_state_mirror_host_tests PRIVATE CS80Reference)
        add_test(NAME cs80_state_mirror_host_tests COMMAND cs80_state_mirror_host_tests)
        set_tests_properties(cs80_state_mirror_host_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_scala_session_admission_tests tests/ScalaSessionAdmissionTests.cpp)
        target_include_directories(cs80_scala_session_admission_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_scala_session_admission_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_scala_session_admission_tests PRIVATE CS80Reference)
        add_test(NAME cs80_scala_session_admission_tests COMMAND cs80_scala_session_admission_tests)
        set_tests_properties(cs80_scala_session_admission_tests PROPERTIES TIMEOUT 120)
        add_executable(cs80_ui_control_mailbox_tests tests/UiControlMailboxTests.cpp)
        target_include_directories(cs80_ui_control_mailbox_tests PRIVATE src
            "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
        target_compile_definitions(cs80_ui_control_mailbox_tests PRIVATE
            "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
        target_link_libraries(cs80_ui_control_mailbox_tests PRIVATE CS80Reference)
        add_test(NAME cs80_ui_control_mailbox_tests COMMAND cs80_ui_control_mailbox_tests)
        set_tests_properties(cs80_ui_control_mailbox_tests PROPERTIES TIMEOUT 120)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            add_executable(cs80_voice_inspection_allocation_tests tests/VoiceInspectionAllocationTests.cpp)
            target_include_directories(cs80_voice_inspection_allocation_tests PRIVATE src
                "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
            target_compile_definitions(cs80_voice_inspection_allocation_tests PRIVATE
                "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
            target_link_libraries(cs80_voice_inspection_allocation_tests PRIVATE CS80Reference)
            target_link_options(cs80_voice_inspection_allocation_tests PRIVATE
                -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
                -Wl,--wrap=aligned_alloc -Wl,--wrap=posix_memalign)
            add_test(NAME cs80_voice_inspection_allocation_tests COMMAND cs80_voice_inspection_allocation_tests)
            set_tests_properties(cs80_voice_inspection_allocation_tests PROPERTIES TIMEOUT 120)
            add_executable(cs80_audio_callback_allocation_tests tests/AudioCallbackAllocationTests.cpp)
            target_include_directories(cs80_audio_callback_allocation_tests PRIVATE src
                "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
            target_compile_definitions(cs80_audio_callback_allocation_tests PRIVATE
                "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
            target_link_libraries(cs80_audio_callback_allocation_tests PRIVATE CS80Reference)
            target_link_options(cs80_audio_callback_allocation_tests PRIVATE
                -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
                -Wl,--wrap=aligned_alloc -Wl,--wrap=posix_memalign)
            add_test(NAME cs80_audio_callback_allocation_tests COMMAND cs80_audio_callback_allocation_tests)
            set_tests_properties(cs80_audio_callback_allocation_tests PROPERTIES TIMEOUT 120)
        endif()
        include(cmake/HostAutomationTests.cmake)
        cs80_add_main_automation_witnesses("${CMAKE_CURRENT_SOURCE_DIR}" "${CS80_VST3_SDK}")
    endif()

    # Headless UI snapshot tool, built from the same editor/processor sources.
    juce_add_console_app(CS80Snapshots PRODUCT_NAME "CS80Snapshots")
    target_sources(CS80Snapshots PRIVATE
        ${CS80_PLUGIN_SOURCES}
        src/tools/ui_snapshot.cpp
    )
    target_include_directories(CS80Snapshots PRIVATE src)
    target_link_libraries(CS80Snapshots PRIVATE
        LibXml2::LibXml2
        cs80_core
        ${CMAKE_DL_LIBS}
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

    # Opening sound recall through the real browser and processor, without a native peer.
    add_test(NAME cs80_preset_revert_tests COMMAND CS80Snapshots --preset-revert-tests)
    set_tests_properties(cs80_preset_revert_tests PROPERTIES TIMEOUT 120)
    # Existing modern state/recall and A/B suites, independently of the full UI smoke.
    add_test(NAME cs80_modern_recall_host_tests COMMAND CS80Snapshots --modern-recall-host-tests)
    set_tests_properties(cs80_modern_recall_host_tests PROPERTIES TIMEOUT 120)
    # Existing successful/normalized user-file recall suites, without native UI.
    add_test(NAME cs80_preset_file_recall_tests COMMAND CS80Snapshots --preset-file-recall-tests)
    set_tests_properties(cs80_preset_file_recall_tests PROPERTIES TIMEOUT 120)
    # Complete-document and security admission of the exact user-file snapshot.
    add_test(NAME cs80_preset_xml_tests COMMAND CS80Snapshots --preset-xml-tests)
    set_tests_properties(cs80_preset_xml_tests PROPERTIES TIMEOUT 60)
    # Missing/corrupt user-file LOAD and audition, through real browser handlers.
    add_test(NAME cs80_preset_load_tests COMMAND CS80Snapshots --preset-load-tests)
    set_tests_properties(cs80_preset_load_tests PROPERTIES TIMEOUT 60)
    # Atomic save/failure storage checks, without modal overwrite dialogs.
    add_test(NAME cs80_preset_save_storage_tests COMMAND CS80Snapshots --preset-save-storage-tests)
    set_tests_properties(cs80_preset_save_storage_tests PROPERTIES TIMEOUT 60)
    # Duplicate-name file provenance through actual browser/processor workflows.
    add_test(NAME cs80_preset_identity_tests COMMAND CS80Snapshots --preset-identity-tests)
    set_tests_properties(cs80_preset_identity_tests PROPERTIES TIMEOUT 60)
    # Real browser components and a temporary catalogue, without a native peer.
    add_test(NAME cs80_preset_refresh_tests COMMAND CS80Snapshots --preset-refresh-tests)
    set_tests_properties(cs80_preset_refresh_tests PROPERTIES TIMEOUT 60)
    # Pending search input through real TextEditor and list keyboard handlers, no native peer.
    add_test(NAME cs80_preset_search_tests COMMAND CS80Snapshots --preset-search-tests)
    set_tests_properties(cs80_preset_search_tests PROPERTIES TIMEOUT 60)
    # Preference controls and fresh-process persistence, no native peer or real preferences.
    add_test(NAME cs80_preset_preferences_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_preset_preferences.py
                --executable $<TARGET_FILE:CS80Snapshots>)
    set_tests_properties(cs80_preset_preferences_tests PROPERTIES TIMEOUT 90)
    # Explicit pointer choice of a restored selection through real JUCE rows, no native peer.
    add_test(NAME cs80_preset_click_tests COMMAND CS80Snapshots --preset-click-tests)
    set_tests_properties(cs80_preset_click_tests PROPERTIES TIMEOUT 60)
    # Real processor/FIFO ownership tests without constructing an editor or
    # native peer. The existing full UI smoke test remains independent.
    add_test(NAME cs80_undo_replay_gesture_tests COMMAND CS80Snapshots --undo-replay-gesture-tests)
    set_tests_properties(cs80_undo_replay_gesture_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_hardware_source_history_tests COMMAND CS80Snapshots --hardware-source-history-tests)
    set_tests_properties(cs80_hardware_source_history_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_patch_history_tests COMMAND CS80Snapshots --patch-history-tests)
    set_tests_properties(cs80_patch_history_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_mpe_released_target_host_tests COMMAND CS80Snapshots --mpe-released-target-tests)
    set_tests_properties(cs80_mpe_released_target_host_tests PROPERTIES TIMEOUT 240)
    add_test(NAME cs80_mpe_member_onset_host_tests COMMAND CS80Snapshots --mpe-member-onset-tests)
    set_tests_properties(cs80_mpe_member_onset_host_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_sixteen_voice_release_host_tests COMMAND CS80Snapshots --sixteen-voice-release-tests)
    set_tests_properties(cs80_sixteen_voice_release_host_tests PROPERTIES TIMEOUT 120)
    # Production Studio ring-envelope drag ownership, without a native peer.
    add_test(NAME cs80_ring_curve_gesture_tests COMMAND CS80Snapshots --ring-curve-gesture-tests)
    set_tests_properties(cs80_ring_curve_gesture_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_mod_matrix_route_actions_tests COMMAND CS80Snapshots --mod-matrix-route-actions-tests)
    set_tests_properties(cs80_mod_matrix_route_actions_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_mod_matrix_routing_ui_tests COMMAND CS80Snapshots --mod-matrix-routing-ui-tests)
    set_tests_properties(cs80_mod_matrix_routing_ui_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_mseg_point_values_tests COMMAND CS80Snapshots --mseg-point-values-tests)
    set_tests_properties(cs80_mseg_point_values_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_mseg_shape_copy_tests COMMAND CS80Snapshots --mseg-shape-copy-tests)
    set_tests_properties(cs80_mseg_shape_copy_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_mseg_editor_lifecycle_tests COMMAND CS80Snapshots --mseg-editor-lifecycle-tests)
    set_tests_properties(cs80_mseg_editor_lifecycle_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_modern_slider_gesture_tests COMMAND CS80Snapshots --modern-slider-gesture-tests)
    set_tests_properties(cs80_modern_slider_gesture_tests PROPERTIES TIMEOUT 60)
    # QWERTY overlap through real handlers and audio handoff, without a native peer.
    add_test(NAME cs80_qwerty_note_ownership_tests COMMAND CS80Snapshots --qwerty-note-ownership-tests)
    set_tests_properties(cs80_qwerty_note_ownership_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_toolbar_menu_lifetime_tests COMMAND CS80Snapshots --toolbar-menu-lifetime-tests)
    set_tests_properties(cs80_toolbar_menu_lifetime_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_snapshot_cli_tests COMMAND "${Python3_EXECUTABLE}"
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/SnapshotCliTests.py"
             "$<TARGET_FILE:CS80Snapshots>")
    set_tests_properties(cs80_snapshot_cli_tests PROPERTIES TIMEOUT 30)
    add_test(NAME cs80_mts_dormant_engine_host_tests COMMAND CS80Snapshots --mts-dormant-engine-tests)
    set_tests_properties(cs80_mts_dormant_engine_host_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_controller_reset_host_tests COMMAND CS80Snapshots --controller-reset-tests)
    set_tests_properties(cs80_controller_reset_host_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_voice_activity_host_tests COMMAND CS80Snapshots --voice-activity-tests)
    set_tests_properties(cs80_voice_activity_host_tests PROPERTIES TIMEOUT 120)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test(NAME cs80_voice_activity_ui_tests
                 COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-ui-smoke-linux.sh
                         $<TARGET_FILE:CS80Snapshots> --voice-activity-ui-tests)
    else()
        add_test(NAME cs80_voice_activity_ui_tests COMMAND CS80Snapshots --voice-activity-ui-tests)
    endif()
    set_tests_properties(cs80_voice_activity_ui_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_keyboard_handoff_host_tests COMMAND CS80Snapshots --keyboard-handoff-tests)
    set_tests_properties(cs80_keyboard_handoff_host_tests PROPERTIES TIMEOUT 120)
    # Native components only, with menu callbacks observed rather than opening
    # a popup/native peer. The full UI smoke remains the popup rendering gate.
    add_test(NAME cs80_switch_context_tests COMMAND CS80Snapshots --switch-context-tests)
    set_tests_properties(cs80_switch_context_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_studio_lfo_context_tests COMMAND CS80Snapshots --studio-lfo-context-tests)
    set_tests_properties(cs80_studio_lfo_context_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_studio_switch_context_tests COMMAND CS80Snapshots --studio-switch-context-tests)
    set_tests_properties(cs80_studio_switch_context_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_parameter_combo_context_tests COMMAND CS80Snapshots --parameter-combo-context-tests)
    set_tests_properties(cs80_parameter_combo_context_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_context_menu_keyboard_tests COMMAND CS80Snapshots --context-menu-keyboard-tests)
    set_tests_properties(cs80_context_menu_keyboard_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_typed_value_unit_tests COMMAND CS80Snapshots --typed-value-unit-tests)
    set_tests_properties(cs80_typed_value_unit_tests PROPERTIES TIMEOUT 60)
    add_test(NAME cs80_keyboard_tuning_ui_tests COMMAND CS80Snapshots --keyboard-tuning-ui-tests)
    set_tests_properties(cs80_keyboard_tuning_ui_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_transient_control_tests COMMAND CS80Snapshots --transient-control-tests)
    set_tests_properties(cs80_transient_control_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_performance_wheel_keyboard_tests COMMAND CS80Snapshots --performance-wheel-keyboard-tests)
    set_tests_properties(cs80_performance_wheel_keyboard_tests PROPERTIES TIMEOUT 120)

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
    add_test(NAME cs80_main_vst3_only_automation_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/MainVst3OnlyAutomationTests.py"
        --source "${CMAKE_CURRENT_SOURCE_DIR}"
        --juce-wrappers "${CS80_WRAPPER_JUCE_MODULES}/juce_audio_plugin_client"
        --tolerances "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/tolerances-v2/main-vst3-only-automation.json"
        --cmake "${CMAKE_COMMAND}")
    set_tests_properties(cs80_main_vst3_only_automation_tests PROPERTIES TIMEOUT 90)
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

# Offline conditional IG00156 filter reference (not whole-instrument truth).
add_executable(cs80_truth_render src/tools/truth_render.cpp)
target_include_directories(cs80_truth_render PRIVATE src)
# Configure reruns when a provenance input changes, including dirty local edits.
foreach(input IN ITEMS src/tools/truth_render.cpp src/tools/TruthInputSha256.h src/reference/IG00156Network.h
                       src/reference/IG00156Linear.h src/core/Ig00156Filter.h src/core/Dsp.h src/core/FastMath.h
                       docs/IG00156_NODAL_MODEL.md)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
endforeach()
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/truth_render.cpp" TRUTH_RENDER_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/TruthInputSha256.h" TRUTH_INPUT_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/reference/IG00156Network.h" TRUTH_NETWORK_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/reference/IG00156Linear.h" TRUTH_LINEAR_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Ig00156Filter.h" TRUTH_CONTROL_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/docs/IG00156_NODAL_MODEL.md" TRUTH_DOCUMENT_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Dsp.h" TRUTH_DSP_SHA)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/core/FastMath.h" TRUTH_FAST_MATH_SHA)
target_compile_definitions(cs80_truth_render PRIVATE
    TRUTH_RENDER_SHA="${TRUTH_RENDER_SHA}" TRUTH_INPUT_SHA="${TRUTH_INPUT_SHA}" TRUTH_NETWORK_SHA="${TRUTH_NETWORK_SHA}"
    TRUTH_LINEAR_SHA="${TRUTH_LINEAR_SHA}" TRUTH_CONTROL_SHA="${TRUTH_CONTROL_SHA}"
    TRUTH_DOCUMENT_SHA="${TRUTH_DOCUMENT_SHA}"
    TRUTH_DSP_SHA="${TRUTH_DSP_SHA}" TRUTH_FAST_MATH_SHA="${TRUTH_FAST_MATH_SHA}")
if (CS80_BUILD_TESTS)
    add_test(NAME cs80_truth_render_tests
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_truth_render.py
                     --renderer $<TARGET_FILE:cs80_truth_render>)
    set_tests_properties(cs80_truth_render_tests PROPERTIES TIMEOUT 120)
endif()
if (CS80_BUILD_TESTS)
    add_executable(cs80_ig00156_step_diagnostics_tests tests/Ig00156StepDiagnosticsTests.cpp)
    target_include_directories(cs80_ig00156_step_diagnostics_tests PRIVATE src)
    add_test(NAME cs80_ig00156_step_diagnostics_tests COMMAND cs80_ig00156_step_diagnostics_tests)
endif()

if (CS80_BUILD_TESTS)
    add_executable(cs80_truth_input_sha256_tests tests/TruthInputSha256Tests.cpp)
    target_include_directories(cs80_truth_input_sha256_tests PRIVATE src)
    add_test(NAME cs80_truth_input_sha256_tests COMMAND cs80_truth_input_sha256_tests)
endif()

# Offline observations of the real core Adsr; no JUCE or runtime changes.
add_executable(cs80_amplitude_production_trace src/tools/amplitude_production_trace.cpp)
target_include_directories(cs80_amplitude_production_trace PRIVATE src)
foreach(pair IN ITEMS "TRACE|src/tools/amplitude_production_trace.cpp"
                      "DECIMAL|src/tools/DecimalConversion.h"
                      "DSP|src/core/Dsp.h"
                      "HASH|src/tools/TruthInputSha256.h"
                      "CMAKE|CMakeLists.txt")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 key)
    list(GET parts 1 input)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" hash)
    target_compile_definitions(cs80_amplitude_production_trace PRIVATE
        "AMPLITUDE_PRODUCTION_${key}_SHA=\"${hash}\"")
endforeach()
target_compile_definitions(cs80_amplitude_production_trace PRIVATE
    "AMPLITUDE_PRODUCTION_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    "AMPLITUDE_PRODUCTION_BUILD_CONFIG=\"$<CONFIG>\"")
if (MSVC)
    target_compile_options(cs80_amplitude_production_trace PRIVATE /W4 /permissive-)
else()
    target_compile_options(cs80_amplitude_production_trace PRIVATE -Wall -Wextra -Wpedantic)
endif()
if (CS80_BUILD_TESTS)
    add_executable(cs80_amplitude_production_oracle tests/AmplitudeProductionOracle.cpp)
    target_include_directories(cs80_amplitude_production_oracle PRIVATE src)
    if (UNIX)
        add_test(NAME cs80_amplitude_production_comparison_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_amplitude_production_comparison.py
                --renderer $<TARGET_FILE:cs80_amplitude_production_trace>
                --oracle $<TARGET_FILE:cs80_amplitude_production_oracle>)
        set_tests_properties(cs80_amplitude_production_comparison_tests PROPERTIES TIMEOUT 180)
    endif()
endif()

# Offline observations of the real core FilterEnv; no JUCE or runtime changes.
add_executable(cs80_filter_production_trace src/tools/filter_production_trace.cpp)
target_include_directories(cs80_filter_production_trace PRIVATE src)
foreach(pair IN ITEMS "TRACE|src/tools/filter_production_trace.cpp"
                      "DECIMAL|src/tools/DecimalConversion.h"
                      "DSP|src/core/Dsp.h"
                      "HASH|src/tools/TruthInputSha256.h"
                      "CMAKE|CMakeLists.txt")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 key)
    list(GET parts 1 input)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" hash)
    target_compile_definitions(cs80_filter_production_trace PRIVATE
        "FILTER_PRODUCTION_${key}_SHA=\"${hash}\"")
endforeach()
target_compile_definitions(cs80_filter_production_trace PRIVATE
    "FILTER_PRODUCTION_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    "FILTER_PRODUCTION_BUILD_CONFIG=\"$<CONFIG>\"")
if (MSVC)
    target_compile_options(cs80_filter_production_trace PRIVATE /W4 /permissive-)
else()
    target_compile_options(cs80_filter_production_trace PRIVATE -Wall -Wextra -Wpedantic)
endif()
if (CS80_BUILD_TESTS)
    add_executable(cs80_filter_production_oracle tests/FilterProductionOracle.cpp)
    target_include_directories(cs80_filter_production_oracle PRIVATE src)
    if (UNIX)
        add_test(NAME cs80_filter_production_comparison_tests
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_filter_production_comparison.py
                --renderer $<TARGET_FILE:cs80_filter_production_trace>
                --oracle $<TARGET_FILE:cs80_filter_production_oracle>)
        set_tests_properties(cs80_filter_production_comparison_tests PROPERTIES TIMEOUT 180)
    endif()
endif()

# Independent offline conditional IG00151 reference; no audio-path changes.
add_executable(cs80_vca_reference src/tools/vca_reference.cpp)
target_include_directories(cs80_vca_reference PRIVATE src)
foreach(pair IN ITEMS "RENDER|src/tools/vca_reference.cpp"
                      "REFERENCE|src/reference/IG00151Reference.h"
                      "HASH|src/tools/TruthInputSha256.h"
                      "CORE|src/core/Vca.h"
                      "MODEL|docs/IG00151_VCA_MODEL.md"
                      "DOCUMENT|docs/VCA_REFERENCE.md")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 key)
    list(GET parts 1 input)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" hash)
    target_compile_definitions(cs80_vca_reference PRIVATE "VCA_${key}_SHA=\"${hash}\"")
endforeach()
if (CS80_BUILD_TESTS)
    add_test(NAME cs80_vca_reference_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vca_reference.py
                --renderer $<TARGET_FILE:cs80_vca_reference>)
    set_tests_properties(cs80_vca_reference_tests PROPERTIES TIMEOUT 120)
endif()

# Shared offline artifact helper provenance.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/FloatWav.h")
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/FloatWav.h" WAV_HELPER_SHA)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/DecimalConversion.h")
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/DecimalConversion.h" DECIMAL_CONVERSION_SHA)
foreach(target IN ITEMS cs80_truth_render cs80_vca_reference)
    target_compile_definitions(${target} PRIVATE WAV_HELPER_SHA="${WAV_HELPER_SHA}"
        DECIMAL_CONVERSION_SHA="${DECIMAL_CONVERSION_SHA}")
endforeach()
if (CS80_BUILD_TESTS)
    add_executable(cs80_decimal_conversion_tests tests/DecimalConversionTests.cpp)
    target_include_directories(cs80_decimal_conversion_tests PRIVATE src)
    add_test(NAME cs80_decimal_conversion_tests COMMAND cs80_decimal_conversion_tests)
    set_tests_properties(cs80_decimal_conversion_tests PROPERTIES TIMEOUT 30)
    if (UNIX)
        # Exercise the exact Apple fallback on Linux too, without requiring an
        # unavailable floating-point from_chars overload in the native tests.
        add_executable(cs80_decimal_conversion_locale_tests tests/DecimalConversionTests.cpp)
        target_include_directories(cs80_decimal_conversion_locale_tests PRIVATE src)
        target_compile_definitions(cs80_decimal_conversion_locale_tests PRIVATE CS80_FORCE_C_LOCALE_DECIMAL=1)
        add_test(NAME cs80_decimal_conversion_locale_tests COMMAND cs80_decimal_conversion_locale_tests)
        set_tests_properties(cs80_decimal_conversion_locale_tests PROPERTIES TIMEOUT 30)
    endif()
    add_executable(cs80_float_wav_fixture tests/float_wav_fixture.cpp)
    target_include_directories(cs80_float_wav_fixture PRIVATE src)
    add_test(NAME cs80_float_wav_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_float_wav.py
            --filter $<TARGET_FILE:cs80_truth_render> --vca $<TARGET_FILE:cs80_vca_reference>
            --fixture $<TARGET_FILE:cs80_float_wav_fixture>)
    set_tests_properties(cs80_float_wav_tests PROPERTIES TIMEOUT 120)
endif()

# Bounded POSIX endpoint orchestration; preserves both existing stage exporters.
if (CS80_BUILD_TESTS AND UNIX)
    add_test(NAME cs80_reference_chain_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reference_chain.py
            --filter $<TARGET_FILE:cs80_truth_render> --vca $<TARGET_FILE:cs80_vca_reference>)
    set_tests_properties(cs80_reference_chain_tests PROPERTIES TIMEOUT 120)
    add_test(NAME cs80_reference_chain_clock_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reference_chain_clock.py
            --filter $<TARGET_FILE:cs80_truth_render> --vca $<TARGET_FILE:cs80_vca_reference>)
    set_tests_properties(cs80_reference_chain_clock_tests PROPERTIES TIMEOUT 120)
endif()

# Independent analytic amplitude EG plus its existing endpoint-chain composition.
if (CS80_BUILD_TESTS AND UNIX)
    add_test(NAME cs80_amplitude_envelope_reference_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_amplitude_envelope_reference.py
            --filter $<TARGET_FILE:cs80_truth_render> --vca $<TARGET_FILE:cs80_vca_reference>)
    set_tests_properties(cs80_amplitude_envelope_reference_tests PROPERTIES TIMEOUT 120)
endif()

# Independent event-aware constant-input envelope/VCA reference (stdlib only).
if (CS80_BUILD_TESTS AND UNIX AND Python3_VERSION VERSION_GREATER_EQUAL "3.11")
    add_test(NAME cs80_event_aware_envelope_vca_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_event_aware_envelope_vca.py)
    set_tests_properties(cs80_event_aware_envelope_vca_tests PROPERTIES TIMEOUT 120)
endif()

# Independent constant-sine/PWL-voltage ring reference (stdlib only).
if (CS80_BUILD_TESTS AND UNIX AND Python3_VERSION VERSION_GREATER_EQUAL "3.11")
    add_test(NAME cs80_ring_time_reference_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ring_time_reference.py)
    set_tests_properties(cs80_ring_time_reference_tests PROPERTIES TIMEOUT 120)
endif()

# Independent event-aware envelope/VCA with one explicit sine input.
if (CS80_BUILD_TESTS AND UNIX AND Python3_VERSION VERSION_GREATER_EQUAL "3.11")
    add_test(NAME cs80_event_aware_envelope_sine_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_event_aware_envelope_sine.py)
    set_tests_properties(cs80_event_aware_envelope_sine_tests PROPERTIES TIMEOUT 120)
endif()

# Independent fixed-carrier RING into event-aware envelope/VCA.
if (CS80_BUILD_TESTS AND UNIX AND Python3_VERSION VERSION_GREATER_EQUAL "3.11")
    add_test(NAME cs80_event_aware_ring_vca_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_event_aware_ring_vca.py)
    set_tests_properties(cs80_event_aware_ring_vca_tests PROPERTIES TIMEOUT 120)
endif()

# Exact coincident-frequency extension; prior RING/VCA tests remain separate.
if (CS80_BUILD_TESTS AND UNIX AND Python3_VERSION VERSION_GREATER_EQUAL "3.11")
    add_test(NAME cs80_event_aware_ring_vca_coincident_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_event_aware_ring_vca_coincident.py)
    set_tests_properties(cs80_event_aware_ring_vca_coincident_tests PROPERTIES TIMEOUT 120)
endif()

# Independent signed filter EG and its dynamic-filter/amplitude chain composition.
if (CS80_BUILD_TESTS AND UNIX)
    add_test(NAME cs80_filter_envelope_reference_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_filter_envelope_reference.py
            --filter $<TARGET_FILE:cs80_truth_render> --vca $<TARGET_FILE:cs80_vca_reference>)
    set_tests_properties(cs80_filter_envelope_reference_tests PROPERTIES TIMEOUT 120)
endif()
