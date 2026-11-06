# Cross-cutting diagnostics, independent of the window, audio and simulation targets.
set(CPPTRACE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CPPTRACE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CPPTRACE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(CPPTRACE_DISABLE_CXX_20_MODULES ON CACHE BOOL "" FORCE)
set(CPPTRACE_STD_FORMAT OFF CACHE BOOL "" FORCE)
set(CPPTRACE_UNWIND_WITH_WINAPI ON CACHE BOOL "" FORCE)
set(CPPTRACE_GET_SYMBOLS_WITH_NOTHING ON CACHE BOOL "" FORCE)
set(CPPTRACE_DEMANGLE_WITH_NOTHING ON CACHE BOOL "" FORCE)
add_subdirectory("${ENGINE_THIRD_PARTY_DIR}/cpptrace" "${CMAKE_BINARY_DIR}/ThirdParty/cpptrace" EXCLUDE_FROM_ALL)

add_library(EngineDiagnostics STATIC Diagnostics/Internals/CrashSession.cpp)
target_include_directories(EngineDiagnostics PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(EngineDiagnostics PRIVATE cpptrace::cpptrace)
target_compile_definitions(EngineDiagnostics PRIVATE ENGINE_NAME="${engineName}" ENGINE_VERSION="${engineVersion}"
    NOMINMAX WIN32_LEAN_AND_MEAN)
engine_warnings(EngineDiagnostics)

add_executable(EngineCrashReporter Diagnostics/Internals/CrashReporterMain.cpp Diagnostics/Internals/CrashReport.cpp)
target_include_directories(EngineCrashReporter PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(EngineCrashReporter PRIVATE nlohmann_json::nlohmann_json dbghelp user32)
target_compile_definitions(EngineCrashReporter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
engine_warnings(EngineCrashReporter)
set_target_properties(EngineCrashReporter PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Diagnostics/$<CONFIG>")
