include(FetchContent)
if(NOT TARGET Catch2::Catch2WithMain)
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(Catch2)
endif()
FetchContent_GetProperties(Catch2 SOURCE_DIR editor_catch_dir)
list(APPEND CMAKE_MODULE_PATH "${editor_catch_dir}/extras")
include(Catch)
add_executable(EngineEditorTests EditorInfrastructureTests.cpp EditorUiModelTests.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Tests/Support/TempDirectory.cpp")
target_link_libraries(EngineEditorTests PRIVATE EngineEditorSupport Catch2::Catch2WithMain)
if(TARGET EngineEditorUi)
    # ImGui-only frames: no window, GPU context, worker or game startup.
    target_sources(EngineEditorTests PRIVATE InspectorUiTests.cpp)
    target_link_libraries(EngineEditorTests PRIVATE EngineEditorUi)
endif()
engine_warnings(EngineEditorTests)
catch_discover_tests(EngineEditorTests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "engine-editor" TIMEOUT 30)
