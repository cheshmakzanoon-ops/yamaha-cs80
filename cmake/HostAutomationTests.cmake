# Additive Linux witnesses. The caller has already created CS80Reference and
# resolved the official VST3 SDK; no system plugin or baseline is substituted.
function(cs80_add_main_automation_witnesses source_root sdk_root)
    add_executable(cs80_main_vst3_multirecall_state_tests
        "${source_root}/tests/MainVst3MultiRecallStateTests.cpp")
    target_include_directories(cs80_main_vst3_multirecall_state_tests PRIVATE
        "${source_root}/src" "$<TARGET_PROPERTY:CS80Reference,INCLUDE_DIRECTORIES>")
    target_compile_definitions(cs80_main_vst3_multirecall_state_tests PRIVATE
        "$<TARGET_PROPERTY:CS80Reference,COMPILE_DEFINITIONS>")
    target_link_libraries(cs80_main_vst3_multirecall_state_tests PRIVATE CS80Reference)
    add_test(NAME cs80_main_vst3_multirecall_state_tests
        COMMAND cs80_main_vst3_multirecall_state_tests
        "${source_root}/tests/parity/tolerances-v2/main-vst3-multirecall-state.json")
    set_tests_properties(cs80_main_vst3_multirecall_state_tests PROPERTIES TIMEOUT 120)

    # This capture tool must be run unchanged against two independently built
    # modules by CompareVst3MainCompatibility.py. A self-comparison is no gate.
    add_executable(cs80_vst3_main_compatibility_capture
        "${source_root}/tests/Vst3MainCompatibilityCapture.cpp"
        "${sdk_root}/pluginterfaces/base/coreiids.cpp"
        "${sdk_root}/pluginterfaces/base/funknown.cpp"
        "${sdk_root}/public.sdk/source/vst/vstinitiids.cpp")
    target_include_directories(cs80_vst3_main_compatibility_capture PRIVATE
        "${source_root}/src" "${sdk_root}")
    target_link_libraries(cs80_vst3_main_compatibility_capture PRIVATE ${CMAKE_DL_LIBS})
    add_dependencies(cs80_vst3_main_compatibility_capture CS80Reference_VST3)
endfunction()
