engine_use_catch2()
add_executable(EngineEditorTests EditorInfrastructureTests.cpp EditorUiModelTests.cpp ProfilerEditorTests.cpp
    EditorCommandTests.cpp TuningEditorTests.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Tests/Support/TempDirectory.cpp")
target_link_libraries(EngineEditorTests PRIVATE EngineEditorSupport Catch2::Catch2WithMain)
if(TARGET EngineEditorUi)
    # ImGui-only frames: no window, GPU context, worker or game startup.
    target_sources(EngineEditorTests PRIVATE InspectorUiTests.cpp WorkspaceUiTests.cpp)
    target_link_libraries(EngineEditorTests PRIVATE EngineEditorUi)
endif()
engine_warnings(EngineEditorTests)
catch_discover_tests(EngineEditorTests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "engine-editor" TIMEOUT 30)
