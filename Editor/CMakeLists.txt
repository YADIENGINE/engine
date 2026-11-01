if(ENGINE_BUILD_EDITOR)
    if(NOT WIN32)
        message(FATAL_ERROR "Engine editor and worker executables currently support Windows desktop only")
    endif()
    if(NOT ENGINE_BUILD_RUNTIME)
        message(FATAL_ERROR "Engine editor/worker executables require ENGINE_BUILD_RUNTIME=ON")
    endif()
endif()

add_library(EngineEditorApi STATIC Api/EditorScene.cpp Api/SceneParameter.cpp Api/SceneRegistry.cpp
    Api/SceneAutoRegistration.cpp Api/GraphicAsset.cpp)
# Editör include'ları engine köküne göredir (#include "Editor/Api/..."); Core'unkiler Core'a göre.
target_include_directories(EngineEditorApi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
get_filename_component(engine_editor_root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
target_compile_definitions(EngineEditorApi PRIVATE "ENGINE_EDITOR_ROOT=\"${engine_editor_root}\"")
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
    Internals/Session/TuningWorkspace.cpp
    Internals/Session/WorkerSnapshots.cpp
    Internals/Session/ProfilerSession.cpp
    Internals/Session/BenchmarkState.cpp
    Internals/Session/FrameCapture.cpp
    Internals/Session/ViewportMark.cpp
    Internals/Session/AgentContext.cpp
    Internals/Agent/AgentProfile.cpp
    Internals/Agent/SessionActivity.cpp
    Internals/Viewport/SceneViewport.cpp
    Internals/Viewport/ViewportInputRouter.cpp
    Internals/Diagnostics/EditorDiagnostics.cpp
    Internals/Settings/EditorPreferences.cpp
    Internals/Settings/EditorSettingsStore.cpp
)
target_link_libraries(EngineEditorSupport PUBLIC EngineEditorApi EngineProfiler nlohmann_json::nlohmann_json PRIVATE EngineStorage)
engine_warnings(EngineEditorSupport)
if(WIN32)
    target_sources(EngineEditorSupport PRIVATE
        Internals/Platform/Windows/BuildLease.cpp
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
        Worker/WorkerProfiler.cpp Worker/GlowBenchmark.cpp Worker/RenderGallery.cpp)
    target_link_libraries(EngineWorkerRuntime PUBLIC Engine EngineEditorApi EngineEditorSupport EngineProfilerRuntime)
    engine_warnings(EngineWorkerRuntime)

    function(engine_editor_escape_json result value)
        string(REPLACE "\\" "\\\\" escaped "${value}")
        string(REPLACE "\"" "\\\"" escaped "${escaped}")
        set(${result} "${escaped}" PARENT_SCOPE)
    endfunction()

    # Self-registering sources: OBJECT libraries retain registration-only translation units without whole-archive flags.
    function(engine_add_worker_objects target objects libraries)
        add_library(${objects} OBJECT ${ARGN})
        target_link_libraries(${objects} PRIVATE EngineWorkerRuntime ${libraries})
        # Include project-local worker settings even when assigned after this helper returns.
        target_include_directories(${objects} PRIVATE "$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>")
        target_compile_definitions(${objects} PRIVATE "$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>")
        set_target_properties(${objects} PROPERTIES CXX_EXTENSIONS OFF)
        engine_warnings(${objects})
        target_link_libraries(${target} PRIVATE ${objects})
    endfunction()

    # A project supplies its worker process (own main via RunEditorWorker); EngineEditor finds it through the
    # EngineEditor.project.json recipe generated beside both executables.
    # With no sources (a fresh project) the engine's starter worker stands in until the first source is added.
    # GRAPHIC_SOURCES: drawing code under Assets/Graphics when no library in LIBRARIES already holds it.
    function(engine_add_editor_worker target)
        cmake_parse_arguments(WORKER "" "NAME;ASSETS" "SOURCES;LIBRARIES;SCENE_DIRS;GRAPHIC_SOURCES" ${ARGN})
        set(useStarter FALSE)
        if(NOT WORKER_NAME)
            set(WORKER_NAME "${PROJECT_NAME} scenes")
        endif()
        if(NOT WORKER_SOURCES)
            message(STATUS "${target}: no scene sources yet; building the engine starter worker")
            set(WORKER_SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Worker/StarterWorker.cpp")
            set(useStarter TRUE)
        endif()
        add_executable(${target} ${WORKER_SOURCES})
        target_link_libraries(${target} PRIVATE EngineWorkerRuntime ${WORKER_LIBRARIES})
        set_target_properties(${target} PROPERTIES
            CXX_EXTENSIONS OFF RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Editor/$<CONFIG>")
        engine_warnings(${target})
        set(sceneDirectoryIndex 0)
        foreach(sceneDirectory IN LISTS WORKER_SCENE_DIRS)
            get_filename_component(sceneDirectory "${sceneDirectory}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            file(GLOB sceneSources CONFIGURE_DEPENDS "${sceneDirectory}/*.cpp")
            if(sceneSources)
                engine_add_worker_objects(${target} "${target}Scenes${sceneDirectoryIndex}" "${WORKER_LIBRARIES}"
                    ${sceneSources})
            endif()
            math(EXPR sceneDirectoryIndex "${sceneDirectoryIndex} + 1")
        endforeach()
        # Asset library: X.asset.cpp registrations reach only this worker, so games never carry the catalog.
        get_filename_component(engineRoot "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
        file(GLOB_RECURSE assetSources CONFIGURE_DEPENDS "${engineRoot}/Assets/Graphics/*.asset.cpp")
        set(assetsDirectory "")
        if(WORKER_ASSETS)
            get_filename_component(assetsDirectory "${WORKER_ASSETS}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            file(GLOB_RECURSE projectAssetSources CONFIGURE_DEPENDS "${assetsDirectory}/Graphics/*.asset.cpp")
            list(APPEND assetSources ${projectAssetSources})
            engine_copy_assets(${target} "${assetsDirectory}")
        endif()
        if(assetSources OR WORKER_GRAPHIC_SOURCES)
            engine_add_worker_objects(${target} "${target}Assets" "${WORKER_LIBRARIES}"
                ${assetSources} ${WORKER_GRAPHIC_SOURCES})
        endif()
        if(useStarter)
            engine_copy_starter_font(${target})
        endif()

        engine_editor_escape_json(name "${WORKER_NAME}")
        engine_editor_escape_json(cmake "${CMAKE_COMMAND}")
        engine_editor_escape_json(build_dir "${CMAKE_BINARY_DIR}")
        engine_editor_escape_json(source_dir "${CMAKE_SOURCE_DIR}")
        engine_editor_escape_json(engine_dir "${engineRoot}")
        engine_editor_escape_json(assets_dir "${assetsDirectory}")
        engine_editor_escape_json(python_path "${Python3_EXECUTABLE}") # Runs the Assets panel's file actions
        file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/Editor/$<CONFIG>/EngineEditor.project.json" CONTENT
"{
  \"version\": 1,
  \"name\": \"${name}\",
  \"buildDirectory\": \"${build_dir}\",
  \"sourceDirectory\": \"${source_dir}\",
  \"engineDirectory\": \"${engine_dir}\",
  \"assetsDirectory\": \"${assets_dir}\",
  \"python\": \"${python_path}\",
  \"worker\": {
    \"executable\": \"$<TARGET_FILE:${target}>\",
    \"arguments\": [], \"directory\": \"$<TARGET_FILE_DIR:${target}>\", \"environment\": {}
  },
  \"build\": {
    \"executable\": \"${cmake}\",
    \"arguments\": [\"--build\", \"${build_dir}\", \"--config\", \"$<CONFIG>\", \"--target\", \"${target}\"],
    \"directory\": \"${build_dir}\", \"environment\": {}
  }
}
")
    endfunction()
endif()


if(ENGINE_BUILD_EDITOR)
    add_library(EngineEditorUi STATIC
        Ui/Commands/EditorCommands.cpp
        Ui/Commands/EditorCommandPalette.cpp
        Ui/Style/EditorTheme.cpp
        Ui/Widgets/EditorWidgets.cpp
        Ui/Widgets/EditorAgentSetup.cpp
        Ui/Widgets/InspectorWidgets.cpp
        Ui/Widgets/EditorPlaybackControls.cpp
        Ui/Shell/EditorShell.cpp
        Ui/Workspace/EditorWorkspace.cpp
        Ui/Panels/Assets/AssetsPanel.cpp
        Ui/Panels/Audio/AudioPreviewPanel.cpp
        Ui/Panels/Catalog/CatalogPanel.cpp
        Ui/Panels/Viewport/ViewportPanel.cpp
        Ui/Panels/Inspector/InspectorPanel.cpp
        Ui/Panels/Tuning/TuningPanel.cpp
        Ui/Panels/Output/OutputPanel.cpp
        Ui/Panels/Console/ConsolePanel.cpp)
    target_sources(EngineEditorUi PRIVATE Ui/Panels/Profiler/ProfilerPanel.cpp)
    target_link_libraries(EngineEditorUi PUBLIC EngineToolUi EngineTerminalUi EngineEditorSupport)
    engine_warnings(EngineEditorUi)
    add_executable(EngineEditor WIN32 Host/Main.cpp Host/AssetCommandRunner.cpp Host/AssetThumbnailStore.cpp Host/EditorApplication.cpp Host/FramePng.cpp Host/ViewportDisplay.cpp
        Host/ProjectListLauncher.cpp Application/EditorController.cpp Application/EditorProfilerCommands.cpp
        Application/EditorControlService.cpp Application/WorkerBuild.cpp Application/WorkerProcess.cpp)
    # Resolve raylib's CloseWindow/ShowCursor before the transitive user32 import library.
    target_link_libraries(EngineEditor PRIVATE Engine EngineEditorSupport EngineEditorUi raylib shell32)
    target_compile_definitions(EngineEditor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    set_target_properties(EngineEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${editor_output}")
    engine_warnings(EngineEditor)
endif()

if(ENGINE_BUILD_EDITOR_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
