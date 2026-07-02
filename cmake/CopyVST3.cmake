# Best-effort copy of the built VST3 bundle into the system VST3 folder.
# Invoked post-build:  cmake -DSRC=<bundle> -DDEST=<folder> -P CopyVST3.cmake
# Never fails the build; prints clear instructions when the copy is denied
# (SPEC section 2: run Tools/grant_vst3_write.ps1 once from elevated PowerShell).

get_filename_component(SRC_ABS "${SRC}" ABSOLUTE)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SRC_ABS}" "${DEST}/Lumen.vst3"
    RESULT_VARIABLE copy_result
    ERROR_VARIABLE copy_error
)

if(copy_result EQUAL 0)
    message(STATUS "Lumen.vst3 copied to ${DEST}")
else()
    message(WARNING
        "Could not copy Lumen.vst3 to ${DEST} (${copy_error}). "
        "One-time fix: run Tools/grant_vst3_write.ps1 from an ELEVATED PowerShell, then rebuild. "
        "Manual copy: '${SRC_ABS}' -> '${DEST}/Lumen.vst3'")
endif()
