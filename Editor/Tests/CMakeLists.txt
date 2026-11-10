engine_use_catch2()
add_executable(EngineEditorTests NotesTests.cpp AssetLibraryTests.cpp PixelatorTests.cpp PixelatorLintTests.cpp PixelatorViewsTests.cpp QuickOpenTests.cpp AudioPreviewTests.cpp ControlChannelTests.cpp EditorInfrastructureTests.cpp EditorControlTests.cpp EditorUiModelTests.cpp ProfilerEditorTests.cpp
    EditorCommandTests.cpp GitTests.cpp OneSceneTests.cpp TuningEditorTests.cpp WorkerSimulationTests.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../Worker/WorkerScene.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Tests/Support/TempDirectory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Tests/Simulation/ToySimulation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Tests/Simulation/ToyTimeline.cpp")
target_link_libraries(EngineEditorTests PRIVATE EngineEditorSupport EngineAudio Catch2::Catch2WithMain)
if(TARGET EngineEditorUi)
    # ImGui-only frames: no window, GPU context, worker or game startup.
    target_sources(EngineEditorTests PRIVATE NotesUiTests.cpp EditorActionTests.cpp EditorContextTests.cpp EditorPaletteTests.cpp InspectorUiTests.cpp
        WorkspaceUiTests.cpp)
    target_link_libraries(EngineEditorTests PRIVATE EngineEditorUi)
    # Opt-in real-renderer screenshots of the panels; needs a desktop/GPU, so not a CTest (EditorUiSmoke.cpp).
    add_executable(EngineEditorUiSmoke EXCLUDE_FROM_ALL EditorUiSmoke.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../Host/PixelatorTextures.cpp")
    target_link_libraries(EngineEditorUiSmoke PRIVATE EngineEditorUi Engine raylib)
    engine_warnings(EngineEditorUiSmoke)
    # Opt-in checks of what the GPU really draws: shaders and GL state the NullDevice cannot see (GpuPixelCheck.cpp).
    add_executable(EngineGpuPixelCheck EXCLUDE_FROM_ALL GpuPixelCheck.cpp)
    target_link_libraries(EngineGpuPixelCheck PRIVATE Engine raylib)
    target_include_directories(EngineGpuPixelCheck PRIVATE "${ENGINE_THIRD_PARTY_DIR}/raylib/src")
    engine_warnings(EngineGpuPixelCheck)
    target_compile_definitions(EngineGpuPixelCheck PRIVATE
        ENGINE_TEST_FONT="${ENGINE_THIRD_PARTY_DIR}/NotoSans/NotoSans-Regular.ttf")
    # Opt-in check with a real Game and a hidden window: Game::Run shuts scenes down while they are alive.
    add_executable(EngineSceneOwnershipCheck EXCLUDE_FROM_ALL SceneOwnershipCheck.cpp)
    target_link_libraries(EngineSceneOwnershipCheck PRIVATE Engine)
    engine_warnings(EngineSceneOwnershipCheck)
endif()
engine_warnings(EngineEditorTests)
target_compile_definitions(EngineEditorTests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
engine_enable_test_crash_reports(EngineEditorTests)
catch_discover_tests(EngineEditorTests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS "engine-editor" TIMEOUT 30)
