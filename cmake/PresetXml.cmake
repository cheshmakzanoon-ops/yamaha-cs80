# A strict, non-network XML admission boundary for user preset files only.
# Keep optional libxml2 functionality and its runtime dependencies out of the
# plugin. Offline builds may set FETCHCONTENT_SOURCE_DIR_CS80_LIBXML2 to a
# checkout of the exact revision below (the source root contains VERSION).
function(cs80_add_preset_xml_parser)
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF)
    foreach(feature CATALOG DEBUG DOCS HTML HTTP ICONV ICU ISO8859X LEGACY MODULES
                    OUTPUT PATTERN PROGRAMS PUSH PYTHON READLINE REGEXPS SAX1
                    TESTS TLS VALID XINCLUDE XPATH ZLIB C14N HISTORY READER SCHEMAS
                    SCHEMATRON THREAD_ALLOC WRITER XPTR RELAXNG)
        set(LIBXML2_WITH_${feature} OFF)
    endforeach()
    set(LIBXML2_WITH_THREADS ON)
    FetchContent_Declare(cs80_libxml2
        GIT_REPOSITORY https://github.com/GNOME/libxml2.git
        # Official libxml2 2.15.4, including its September 2026 security fixes.
        GIT_TAG 96498992efa48d52b0e8b83058bd88dbdaf153c1)
    FetchContent_MakeAvailable(cs80_libxml2)
    # Do not install the upstream library/headers or export its symbols into a
    # host that may already use libxml2. It is linked statically into our binary.
    set_property(DIRECTORY "${cs80_libxml2_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
    set_target_properties(LibXml2 PROPERTIES C_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON)
endfunction()
cs80_add_preset_xml_parser()
