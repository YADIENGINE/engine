function(engine_enable_crash_reports target)
    if(NOT WIN32)
        return()
    endif()
    target_link_libraries(${target} PRIVATE EngineDiagnostics)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:EngineCrashReporter>")
    target_link_options(${target} PRIVATE /DEBUG:FULL "$<$<CONFIG:Release,RelWithDebInfo>:/OPT:REF;/OPT:ICF>")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:EngineCrashReporter>"
            "$<TARGET_FILE_DIR:${target}>/EngineCrashReporter.exe" VERBATIM)
endfunction()

# engine_enable_test_crash_reports(<target>): adds a Catch2 listener without replacing Catch2's main or SEH handler.
function(engine_enable_test_crash_reports target)
    if(NOT WIN32)
        return()
    endif()
    engine_enable_crash_reports(${target})
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Internals/CrashTestListener.cpp")
endfunction()
