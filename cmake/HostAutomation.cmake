# Generate only the reviewed wrapper translation unit in the build tree. The
# fetched/local JUCE checkout remains untouched, including when builds share it.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch-juce-automation.py")
get_target_property(_cs80_vst3_sources juce_audio_plugin_client_VST3 INTERFACE_SOURCES)
set(_cs80_patched_vst3_sources)
set(_cs80_patched_vst3 FALSE)
foreach(_cs80_source IN LISTS _cs80_vst3_sources)
    if(_cs80_source MATCHES "/juce_audio_plugin_client_VST3\\.cpp$")
        set(_cs80_wrapper "${CMAKE_CURRENT_BINARY_DIR}/cs80-juce-wrappers/juce_audio_plugin_client_VST3.cpp")
        execute_process(COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch-juce-automation.py"
            "${_cs80_source}" "${_cs80_wrapper}"
            RESULT_VARIABLE _cs80_patch_result ERROR_VARIABLE _cs80_patch_error)
        if(NOT _cs80_patch_result EQUAL 0)
            message(FATAL_ERROR "CS-80 host automation wrapper review required: ${_cs80_patch_error}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_cs80_source}")
        list(APPEND _cs80_patched_vst3_sources "${_cs80_wrapper}")
        set(_cs80_patched_vst3 TRUE)
    else()
        list(APPEND _cs80_patched_vst3_sources "${_cs80_source}")
    endif()
endforeach()
if(NOT _cs80_patched_vst3)
    message(FATAL_ERROR "JUCE VST3 wrapper source was not found; sample offsets would be lost")
endif()
set_property(TARGET juce_audio_plugin_client_VST3 PROPERTY INTERFACE_SOURCES "${_cs80_patched_vst3_sources}")
target_include_directories(juce_audio_plugin_client_VST3 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
