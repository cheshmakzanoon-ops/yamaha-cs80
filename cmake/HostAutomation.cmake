# Generate reviewed wrapper translation units only in the build tree. The
# fetched/local JUCE checkout remains untouched, including shared checkouts.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch-juce-automation.py")
foreach(_cs80_format IN ITEMS VST3 AU)
    set(_cs80_target "juce_audio_plugin_client_${_cs80_format}")
    if(NOT TARGET ${_cs80_target})
        continue()
    endif()
    if(_cs80_format STREQUAL "VST3")
        # JUCE lists a .mm trampoline on macOS so its VST3 implementation is
        # compiled as Objective-C++. Retain that native language in the
        # generated target source instead of replacing it with a .cpp file.
        set(_cs80_filenames "juce_audio_plugin_client_VST3.cpp" "juce_audio_plugin_client_VST3.mm")
    else()
        set(_cs80_filenames "juce_audio_plugin_client_AU_1.mm")
    endif()
    get_target_property(_cs80_sources ${_cs80_target} INTERFACE_SOURCES)
    set(_cs80_patched_sources)
    set(_cs80_patched_count 0)
    foreach(_cs80_source IN LISTS _cs80_sources)
        get_filename_component(_cs80_source_name "${_cs80_source}" NAME)
        if(_cs80_source_name IN_LIST _cs80_filenames)
            set(_cs80_wrapper "${CMAKE_CURRENT_BINARY_DIR}/cs80-juce-wrappers/${_cs80_source_name}")
            execute_process(COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/patch-juce-automation.py"
                "${_cs80_source}" "${_cs80_wrapper}"
                RESULT_VARIABLE _cs80_patch_result ERROR_VARIABLE _cs80_patch_error)
            if(NOT _cs80_patch_result EQUAL 0)
                message(FATAL_ERROR "CS-80 host automation wrapper review required: ${_cs80_patch_error}")
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_cs80_source}")
            if(_cs80_source_name STREQUAL "juce_audio_plugin_client_VST3.mm")
                get_filename_component(_cs80_original_directory "${_cs80_source}" DIRECTORY)
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                    "${_cs80_original_directory}/juce_audio_plugin_client_VST3.cpp")
            endif()
            list(APPEND _cs80_patched_sources "${_cs80_wrapper}")
            math(EXPR _cs80_patched_count "${_cs80_patched_count} + 1")
        else()
            list(APPEND _cs80_patched_sources "${_cs80_source}")
        endif()
    endforeach()
    if(_cs80_patched_count EQUAL 0)
        message(FATAL_ERROR "JUCE ${_cs80_format} wrapper source not found; sample offsets would be lost")
    elseif(NOT _cs80_patched_count EQUAL 1)
        message(FATAL_ERROR "JUCE ${_cs80_format}: expected exactly one reviewed wrapper translation unit")
    endif()
    set_property(TARGET ${_cs80_target} PROPERTY INTERFACE_SOURCES "${_cs80_patched_sources}")
    target_include_directories(${_cs80_target} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
endforeach()
