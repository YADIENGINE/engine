if(ENGINE_BUILD_EDITOR)
    if(NOT WIN32)
        message(FATAL_ERROR "Engine editor and worker executables currently support Windows desktop only")
    endif()
    if(NOT ENGINE_BUILD_RUNTIME)
        message(FATAL_ERROR "Engine editor/worker executables require ENGINE_BUILD_RUNTIME=ON")
    endif()
endif()

add_library(EngineEditorApi STATIC Api/EditorScene.cpp Api/SceneParameter.cpp Api/SceneRegistry.cpp
    Api/SceneAutoRegistration.cpp Api/GraphicAsset.cpp Api/SimulationHost.cpp)
# Editör include'ları engine köküne göredir (#include "Editor/Api/..."); Core'unkiler Core'a göre.
target_include_directories(EngineEditorApi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(EngineEditorApi PUBLIC EngineHeadless)
engine_warnings(EngineEditorApi)

add_library(EngineEditorSupport STATIC
    Application/EditorCommandState.cpp
    Internals/Protocol/WorkerProtocol.cpp
    Internals/Protocol/WorkerMessageReader.cpp
    Internals/Protocol/EditorControlProtocol.cpp
    Internals/Session/AssetLibrary.cpp
    Internals/Session/AssetNavigation.cpp
    Internals/Session/AudioAnalysis.cpp
    Internals/Session/AudioPreview.cpp
    Internals/Session/AudioUsage.cpp
    Internals/Session/AudioWaveformCache.cpp
    Internals/Session/EditorProject.cpp
    Internals/Session/RefreshState.cpp
    Internals/Session/TuningBaselines.cpp
    Internals/Session/TuningWorkspace.cpp
    Internals/Session/WorkerSimulation.cpp
    Internals/Session/WorkerSnapshots.cpp
    Internals/Session/ProfilerSession.cpp
    Internals/Session/BenchmarkState.cpp
    Internals/Session/FrameCapture.cpp
    Internals/Session/ViewportMark.cpp
    Internals/Session/MarkPauseState.cpp
    Internals/Session/AgentContext.cpp
    Internals/Session/QuickOpen.cpp
    Internals/Agent/AgentProfile.cpp
    Internals/Agent/ControlLease.cpp
    Internals/Agent/SessionActivity.cpp
    Internals/Git/GitAttention.cpp
    Internals/Git/GitClient.cpp
    Internals/Git/GitOperations.cpp
    Internals/Git/GitParsing.cpp
    Internals/Git/GitSchedule.cpp
    Internals/Git/GitService.cpp
    Internals/Viewport/SceneViewport.cpp
    Internals/Viewport/ViewportInputRouter.cpp
    Internals/Diagnostics/BuildProblems.cpp
    Internals/Diagnostics/EditorDiagnostics.cpp
    Internals/Settings/EditorPreferences.cpp
    Internals/Settings/EditorSettingsStore.cpp
    Internals/Settings/EditorTheme.cpp
)
target_link_libraries(EngineEditorSupport PUBLIC EngineEditorApi EngineProfiler nlohmann_json::nlohmann_json PRIVATE EngineStorage)
engine_warnings(EngineEditorSupport)
if(WIN32)
    target_sources(EngineEditorSupport PRIVATE
        Internals/Platform/Windows/BuildLease.cpp
        Internals/Platform/Windows/CapturedProcess.cpp
        Internals/Platform/Windows/ProcessLaunch.cpp
        Internals/Platform/Windows/ChildProcess.cpp
        Internals/Platform/Windows/CommandLocator.cpp
        Internals/Platform/Windows/ControlChannel.cpp
        Internals/Platform/Windows/EditorControlPipe.cpp
        Internals/Platform/Windows/NativeHandle.cpp
        Internals/Platform/Windows/SharedFrames.cpp
        Internals/Platform/Windows/WindowsText.cpp
    )
    target_link_libraries(EngineEditorSupport PRIVATE EngineStorage user32)
    target_compile_definitions(EngineEditorSupport PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set(editor_output "${CMAKE_BINARY_DIR}/Editor/$<CONFIG>")
if(ENGINE_BUILD_EDITOR)
    add_library(EngineWorkerRuntime STATIC EXCLUDE_FROM_ALL
        Worker/AssetThumbnailGame.cpp Worker/AudioReport.cpp Worker/WorkerScene.cpp Worker/WorkerGame.cpp Worker/WorkerSession.cpp Worker/WorkerHost.cpp
        Worker/WorkerProfiler.cpp Worker/GlowBenchmark.cpp Worker/LightGallery.cpp Worker/PostGallery.cpp
        Worker/RenderGallery.cpp)
    target_link_libraries(EngineWorkerRuntime PUBLIC Engine EngineAssets EngineEditorApi EngineEditorSupport EngineProfilerRuntime)
    engine_warnings(EngineWorkerRuntime)

    include("${CMAKE_CURRENT_LIST_DIR}/WorkerTargets.cmake")
endif()


if(ENGINE_BUILD_EDITOR)
    add_library(EngineEditorUi STATIC
        Ui/Commands/EditorActions.cpp
        Ui/Commands/EditorCommands.cpp
        Ui/Commands/EditorCommandPalette.cpp
        Ui/Style/EditorTheme.cpp
        Ui/Style/EditorPalettePreset.cpp
        Ui/Widgets/EditorWidgets.cpp
        Ui/Widgets/EditorAgentSetup.cpp
        Ui/Widgets/InspectorWidgets.cpp
        Ui/Widgets/Properties.cpp
        Ui/Widgets/PropertyWidgets.cpp
        Ui/Widgets/EditorPlaybackControls.cpp
        Ui/Widgets/EditorAgentActivity.cpp
        Ui/Widgets/EditorGitBar.cpp
        Ui/Shell/EditorShell.cpp
        Ui/Workspace/EditorWorkspace.cpp
        Ui/Panels/Assets/AssetsPanel.cpp
        Ui/Panels/Audio/AudioPreviewPanel.cpp
        Ui/Panels/Catalog/CatalogPanel.cpp
        Ui/Panels/Viewport/ViewportPanel.cpp
        Ui/Panels/Inspector/InspectorPanel.cpp
        Ui/Panels/Inspector/TuningInspector.cpp
        Ui/Panels/Output/OutputPanel.cpp
        Ui/Panels/Agents/AgentsDrawer.cpp
        Ui/Panels/Console/ConsolePanel.cpp
        Ui/Panels/Console/ConsoleSessions.cpp
        Ui/Panels/Console/ConsoleViews.cpp
        Ui/Panels/Git/GitDiffView.cpp
        Ui/Panels/Git/GitDrawer.cpp)
    target_sources(EngineEditorUi PRIVATE Ui/Panels/Profiler/ProfilerPanel.cpp)
    target_link_libraries(EngineEditorUi PUBLIC EngineToolUi EngineTerminalUi EngineEditorSupport)
    engine_warnings(EngineEditorUi)
    # Help → About and the online editor guide.
    target_compile_definitions(EngineEditorUi PRIVATE ENGINE_REPOSITORY="${engineRepository}")

    # Icon drawn from code at configure time (Tools/app_icon.py; never committed). The resource name GLFW_ICON makes
    # Windows show it for the executable and GLFW (raylib) use it for the window and taskbar.
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    set(editor_brand_dir "${CMAKE_CURRENT_BINARY_DIR}/Brand")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Tools/app_icon.py")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../Tools/app_icon.py"
                "${editor_brand_dir}/Icon.ico" --name "${engineName}"
        RESULT_VARIABLE editorIconResult)
    if(NOT editorIconResult EQUAL 0)
        message(FATAL_ERROR "Tools/app_icon.py failed (${editorIconResult})")
    endif()
    file(CONFIGURE OUTPUT "${editor_brand_dir}/EngineEditor.rc" CONTENT "GLFW_ICON ICON \"${editor_brand_dir}/Icon.ico\"\n")

    add_executable(EngineEditor WIN32 Host/Main.cpp Host/AssetCommandRunner.cpp Host/AssetThumbnailStore.cpp Host/EditorApplication.cpp Host/FramePng.cpp Host/GitImages.cpp Host/ViewportDisplay.cpp
        Host/ProjectListLauncher.cpp Application/EditorController.cpp Application/EditorProfilerCommands.cpp
        Application/EditorControlService.cpp Application/WorkerBuild.cpp Application/WorkerProcess.cpp
        "${editor_brand_dir}/EngineEditor.rc")
    # Resolve raylib's CloseWindow/ShowCursor before the transitive user32 import library.
    target_link_libraries(EngineEditor PRIVATE Engine EngineEditorSupport EngineEditorUi raylib shell32)
    target_compile_definitions(EngineEditor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    set_target_properties(EngineEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${editor_output}")
    engine_warnings(EngineEditor)
endif()

if(ENGINE_BUILD_EDITOR_TESTS)
    add_subdirectory(Tests)
endif()
