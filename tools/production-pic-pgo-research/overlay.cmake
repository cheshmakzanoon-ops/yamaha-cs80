# Opt-in research overlay only. Production CMake and the earlier study stay unchanged.
# Passed explicitly as CMAKE_PROJECT_INCLUDE. DEFER runs after root targets exist.
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR OR
   NOT PROJECT_NAME STREQUAL "CS80Reference")
    return()
endif()

function(cs80_apply_pic_pgo_research)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
       NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "13.3.0" OR
       NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
       NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        message(FATAL_ERROR "PIC PGO research requires Linux x86_64 GNU 13.3.0")
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR
       NOT CMAKE_GENERATOR STREQUAL "Ninja" OR
       NOT CS80_BUILD_PLUGIN OR NOT CS80_BUILD_TESTS OR
       CS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY OR NOT CS80_ENABLE_AVX2_FILTER_BATCH)
        message(FATAL_ERROR "PIC PGO research requires the frozen normal Linux configuration")
    endif()
    if(NOT CS80_PIC_PGO_MODE MATCHES "^(baseline|generate|use)$")
        message(FATAL_ERROR "Explicit baseline, generate or use research mode required")
    endif()
    foreach(target IN ITEMS cs80_core CS80Reference_VST3 CS80Reference_Standalone)
        if(NOT TARGET ${target})
            message(FATAL_ERROR "Missing production target ${target}")
        endif()
    endforeach()
    get_target_property(core_type cs80_core TYPE)
    get_target_property(core_pic cs80_core POSITION_INDEPENDENT_CODE)
    get_target_property(core_sources cs80_core SOURCES)
    get_target_property(core_ipo cs80_core INTERPROCEDURAL_OPTIMIZATION)
    get_target_property(core_ipo_release cs80_core INTERPROCEDURAL_OPTIMIZATION_RELEASE)
    if(NOT core_type STREQUAL "STATIC_LIBRARY" OR NOT core_pic OR core_ipo OR core_ipo_release)
        message(FATAL_ERROR "Unexpected production core type/PIC/IPO")
    endif()
    set(expected_sources src/core/Engine.cpp src/core/FactoryPresets.cpp src/core/Ig00156Batch.cpp)
    if(NOT core_sources STREQUAL expected_sources)
        message(FATAL_ERROR "Production core source inventory changed")
    endif()
    set(engine "${CMAKE_CURRENT_SOURCE_DIR}/src/core/Engine.cpp")
    get_source_file_property(existing_options "${engine}" COMPILE_OPTIONS)
    if(existing_options AND NOT existing_options STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Unexpected preexisting Engine source options")
    endif()
    if(NOT CS80_PIC_PGO_MODE STREQUAL "baseline")
        if(NOT IS_ABSOLUTE "${CS80_PIC_PGO_PROFILE_DIR}" OR
           CS80_PIC_PGO_PROFILE_DIR MATCHES "[;\n\r]" OR
           CS80_PIC_PGO_PROFILE_DIR STREQUAL "/")
            message(FATAL_ERROR "Explicit safe absolute research profile directory required")
        endif()
        if(CS80_PIC_PGO_MODE STREQUAL "generate")
            set(profile_options "-fprofile-generate=${CS80_PIC_PGO_PROFILE_DIR}")
        else()
            set(profile_options "-fprofile-use=${CS80_PIC_PGO_PROFILE_DIR}"
                -Werror=missing-profile -Werror=coverage-mismatch)
        endif()
        set_property(SOURCE "${engine}" DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            APPEND PROPERTY COMPILE_OPTIONS ${profile_options})
    endif()
    # Link maps do not instrument the plugin. Never add the generation runtime here.
    foreach(target IN ITEMS CS80Reference_VST3 CS80Reference_Standalone)
        target_link_options(${target} PRIVATE "-Wl,-Map,${CMAKE_BINARY_DIR}/${target}.research.map,--cref")
    endforeach()
endfunction()
cmake_language(DEFER CALL cs80_apply_pic_pgo_research)
