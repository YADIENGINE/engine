engine_use_catch2()
add_executable(EngineEditorTests AssetLibraryTests.cpp QuickOpenTests.cpp AudioPreviewTests.cpp ControlChannelTests.cpp EditorInfrastructureTests.cpp EditorControlTests.cpp EditorUiModelTests.cpp ProfilerEditorTests.cpp
    EditorCommandTests.cpp GitTests.cpp TuningEditorTests.cpp WorkerSimulationTests.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Tests/Support/TempDirectory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Tests/Simulation/ToySimulation.cpp")
target_link_libraries(EngineEditorTests PRIVATE EngineEditorSupport Catch2::Catch2WithMain)
if(TARGET EngineEditorUi)
    # ImGui-only frames: no window, GPU context, worker or game startup.
    target_sources(EngineEditorTests PRIVATE InspectorUiTests.cpp WorkspaceUiTests.cpp)
    target_link_libraries(EngineEditorTests PRIVATE EngineEditorUi)
    # Opt-in real-renderer screenshots of the panels; needs a desktop/GPU, so not a CTest (EditorUiSmoke.cpp).
    add_executable(EngineEditorUiSmoke EXCLUDE_FROM_ALL EditorUiSmoke.cpp)
    target_link_libraries(EngineEditorUiSmoke PRIVATE EngineEditorUi Engine raylib)
    engine_warnings(EngineEditorUiSmoke)
endif()
engine_warnings(EngineEditorTests)
catch_discover_tests(EngineEditorTests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "engine-editor" TIMEOUT 30)
