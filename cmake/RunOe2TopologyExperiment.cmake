# Separate experiment only. Each CTest invocation gets fresh output; no prior
# receipt is deleted/reused, and the runner retains its nonempty-directory guard.
foreach(required OE2_SOURCE_DIR OE2_OUTPUT_ROOT OE2_PYTHON OE2_COMPILER)
    if (NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${OE2_OUTPUT_ROOT}")
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef run_id)
set(run_dir "${OE2_OUTPUT_ROOT}/run-${run_id}")
message(STATUS "Experimental OE2 receipts: ${run_dir}")
execute_process(
    COMMAND "${OE2_PYTHON}" "${OE2_SOURCE_DIR}/tests/diagnostics/run-oe2-topology-experiment.py"
            --repo "${OE2_SOURCE_DIR}" --compiler "${OE2_COMPILER}" --output "${run_dir}"
    RESULT_VARIABLE result)
if (NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Experimental OE2 topology checks failed: ${result}; receipts: ${run_dir}")
endif()
