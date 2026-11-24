# Apply after every add_test, including tests added near the end of the root file.
# Default exclusivity protects GUI timers, persistence, benchmarks, and future tests.
# CTest RUN_SERIAL is a global barrier, including against the allowlisted tests.
function(cs80_make_tests_exclusive directory)
    get_property(tests DIRECTORY "${directory}" PROPERTY TESTS)
    if(tests)
        # All project tests currently live in this directory. Refuse an unknown
        # child roster instead of silently leaving its scheduling unrestricted.
        if(NOT directory STREQUAL CMAKE_CURRENT_SOURCE_DIR)
            message(FATAL_ERROR "Unaudited test directory: ${directory}")
        endif()
        set_tests_properties(${tests} PROPERTIES RUN_SERIAL TRUE)
    endif()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        cs80_make_tests_exclusive("${child}")
    endforeach()
endfunction()
cs80_make_tests_exclusive("${CMAKE_CURRENT_SOURCE_DIR}")

# Linux CI may use --parallel 2. These existing native VST3 witnesses create no
# editor, use sample-index assertions (not wall time), read immutable contracts,
# and write only their distinct family/rate/mode capture directory. No generator,
# UI, persistence, benchmark, or broad boundary/fuzz test is admitted here.
# Enumerate every admitted test: a new family or rate requires a fresh audit.
# The pinned MTS client probes these optional external libraries at startup.
# Their process/global behavior is outside this audit; retain exclusivity on
# machines where either is installed, even when CTest is invoked with -j 2.
if(CS80_BUILD_PLUGIN AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
   AND NOT EXISTS "/usr/local/lib/libMTS.so"
   AND NOT EXISTS "/Library/Application Support/MTS-ESP/libMTS.dylib")
    set(cs80_parallel_automation_tests
        cs80_vst3_portamento_automation_44100_realtime_tests
        cs80_vst3_portamento_automation_44100_offline_tests
        cs80_vst3_portamento_automation_48000_realtime_tests
        cs80_vst3_portamento_automation_48000_offline_tests
        cs80_vst3_portamento_automation_96000_realtime_tests
        cs80_vst3_portamento_automation_96000_offline_tests
        cs80_vst3_master_sustain_automation_44100_realtime_tests
        cs80_vst3_master_sustain_automation_44100_offline_tests
        cs80_vst3_master_sustain_automation_48000_realtime_tests
        cs80_vst3_master_sustain_automation_48000_offline_tests
        cs80_vst3_master_sustain_automation_96000_realtime_tests
        cs80_vst3_master_sustain_automation_96000_offline_tests
        cs80_vst3_initial_bend_automation_44100_realtime_tests
        cs80_vst3_initial_bend_automation_44100_offline_tests
        cs80_vst3_initial_bend_automation_48000_realtime_tests
        cs80_vst3_initial_bend_automation_48000_offline_tests
        cs80_vst3_initial_bend_automation_96000_realtime_tests
        cs80_vst3_initial_bend_automation_96000_offline_tests
        cs80_vst3_kbd_level_low_automation_44100_realtime_tests
        cs80_vst3_kbd_level_low_automation_44100_offline_tests
        cs80_vst3_kbd_level_low_automation_48000_realtime_tests
        cs80_vst3_kbd_level_low_automation_48000_offline_tests
        cs80_vst3_kbd_level_low_automation_96000_realtime_tests
        cs80_vst3_kbd_level_low_automation_96000_offline_tests
        cs80_vst3_kbd_level_high_automation_44100_realtime_tests
        cs80_vst3_kbd_level_high_automation_44100_offline_tests
        cs80_vst3_kbd_level_high_automation_48000_realtime_tests
        cs80_vst3_kbd_level_high_automation_48000_offline_tests
        cs80_vst3_kbd_level_high_automation_96000_realtime_tests
        cs80_vst3_kbd_level_high_automation_96000_offline_tests
        cs80_vst3_kbd_brilliance_automation_44100_realtime_tests
        cs80_vst3_kbd_brilliance_automation_44100_offline_tests
        cs80_vst3_kbd_brilliance_automation_48000_realtime_tests
        cs80_vst3_kbd_brilliance_automation_48000_offline_tests
        cs80_vst3_kbd_brilliance_automation_96000_realtime_tests
        cs80_vst3_kbd_brilliance_automation_96000_offline_tests
        cs80_vst3_initial_touch_automation_44100_realtime_tests
        cs80_vst3_initial_touch_automation_44100_offline_tests
        cs80_vst3_initial_touch_automation_48000_realtime_tests
        cs80_vst3_initial_touch_automation_48000_offline_tests
        cs80_vst3_initial_touch_automation_96000_realtime_tests
        cs80_vst3_initial_touch_automation_96000_offline_tests
        cs80_vst3_after_touch_automation_44100_realtime_tests
        cs80_vst3_after_touch_automation_44100_offline_tests
        cs80_vst3_after_touch_automation_48000_realtime_tests
        cs80_vst3_after_touch_automation_48000_offline_tests
        cs80_vst3_after_touch_automation_96000_realtime_tests
        cs80_vst3_after_touch_automation_96000_offline_tests
        cs80_vst3_pwm_automation_44100_realtime_tests
        cs80_vst3_pwm_automation_44100_offline_tests
        cs80_vst3_pwm_automation_48000_realtime_tests
        cs80_vst3_pwm_automation_48000_offline_tests
        cs80_vst3_pwm_automation_96000_realtime_tests
        cs80_vst3_pwm_automation_96000_offline_tests
        cs80_vst3_vcf_level_automation_44100_realtime_tests
        cs80_vst3_vcf_level_automation_44100_offline_tests
        cs80_vst3_vcf_level_automation_48000_realtime_tests
        cs80_vst3_vcf_level_automation_48000_offline_tests
        cs80_vst3_vcf_level_automation_96000_realtime_tests
        cs80_vst3_vcf_level_automation_96000_offline_tests
        cs80_vst3_vca_sustain_automation_44100_realtime_tests
        cs80_vst3_vca_sustain_automation_44100_offline_tests
        cs80_vst3_vca_sustain_automation_48000_realtime_tests
        cs80_vst3_vca_sustain_automation_48000_offline_tests
        cs80_vst3_vca_sustain_automation_96000_realtime_tests
        cs80_vst3_vca_sustain_automation_96000_offline_tests
        cs80_vst3_filter_il_automation_44100_realtime_tests
        cs80_vst3_filter_il_automation_44100_offline_tests
        cs80_vst3_filter_il_automation_48000_realtime_tests
        cs80_vst3_filter_il_automation_48000_offline_tests
        cs80_vst3_filter_il_automation_96000_realtime_tests
        cs80_vst3_filter_il_automation_96000_offline_tests
        cs80_vst3_filter_al_automation_44100_realtime_tests
        cs80_vst3_filter_al_automation_44100_offline_tests
        cs80_vst3_filter_al_automation_48000_realtime_tests
        cs80_vst3_filter_al_automation_48000_offline_tests
        cs80_vst3_filter_al_automation_96000_realtime_tests
        cs80_vst3_filter_al_automation_96000_offline_tests
        cs80_vst3_envelope_attack_automation_44100_realtime_tests
        cs80_vst3_envelope_attack_automation_44100_offline_tests
        cs80_vst3_envelope_attack_automation_48000_realtime_tests
        cs80_vst3_envelope_attack_automation_48000_offline_tests
        cs80_vst3_envelope_attack_automation_96000_realtime_tests
        cs80_vst3_envelope_attack_automation_96000_offline_tests
        cs80_vst3_envelope_decay_automation_44100_realtime_tests
        cs80_vst3_envelope_decay_automation_44100_offline_tests
        cs80_vst3_envelope_decay_automation_48000_realtime_tests
        cs80_vst3_envelope_decay_automation_48000_offline_tests
        cs80_vst3_envelope_decay_automation_96000_realtime_tests
        cs80_vst3_envelope_decay_automation_96000_offline_tests
        cs80_vst3_envelope_release_automation_44100_realtime_tests
        cs80_vst3_envelope_release_automation_44100_offline_tests
        cs80_vst3_envelope_release_automation_48000_realtime_tests
        cs80_vst3_envelope_release_automation_48000_offline_tests
        cs80_vst3_envelope_release_automation_96000_realtime_tests
        cs80_vst3_envelope_release_automation_96000_offline_tests
        cs80_vst3_envelope_decay_release_combined_automation_44100_realtime_tests
        cs80_vst3_envelope_decay_release_combined_automation_44100_offline_tests
        cs80_vst3_envelope_decay_release_combined_automation_48000_realtime_tests
        cs80_vst3_envelope_decay_release_combined_automation_48000_offline_tests
        cs80_vst3_envelope_decay_release_combined_automation_96000_realtime_tests
        cs80_vst3_envelope_decay_release_combined_automation_96000_offline_tests
        cs80_vst3_sine_noise_automation_44100_realtime_tests
        cs80_vst3_sine_noise_automation_44100_offline_tests
        cs80_vst3_sine_noise_automation_48000_realtime_tests
        cs80_vst3_sine_noise_automation_48000_offline_tests
        cs80_vst3_sine_noise_automation_96000_realtime_tests
        cs80_vst3_sine_noise_automation_96000_offline_tests
)
    foreach(test IN LISTS cs80_parallel_automation_tests)
        if(NOT TEST "${test}")
            message(FATAL_ERROR "Audited parallel automation test is missing: ${test}")
        endif()
        set_tests_properties("${test}" PROPERTIES RUN_SERIAL FALSE)
    endforeach()
endif()
