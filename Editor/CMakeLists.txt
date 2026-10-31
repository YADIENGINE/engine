if(ENGINE_BUILD_EDITOR)
    if(NOT WIN32)
        message(FATAL_ERROR "Engine editor and preview executables currently support Windows desktop only")
    endif()
    if(NOT ENGINE_BUILD_RUNTIME)
        message(FATAL_ERROR "Engine editor/preview executables require ENGINE_BUILD_RUNTIME=ON")
    endif()
endif()

add_library(EngineEditorApi STATIC Api/EditorPreview.cpp Api/PreviewParameter.cpp Api/PreviewRegistry.cpp)
# Editör include'ları engine köküne göredir (#include "Editor/Api/..."); Core'unkiler Core'a göre.
target_include_directories(EngineEditorApi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(EngineEditorApi PUBLIC EngineHeadless)
engine_warnings(EngineEditorApi)

add_library(EngineEditorSupport STATIC
    Application/EditorCommandState.cpp
    Internals/Protocol/PreviewProtocol.cpp
    Internals/Protocol/PreviewMessageReader.cpp
    Internals/Protocol/EditorControlProtocol.cpp
    Internals/Session/EditorProject.cpp
    Internals/Session/RefreshState.cpp
    Internals/Session/TuningWorkspace.cpp
    Internals/Session/WorkerSnapshots.cpp
    Internals/Session/ProfilerSession.cpp
    Internals/Session/BenchmarkState.cpp
    Internals/Viewport/PreviewViewport.cpp
    Internals/Viewport/PreviewInputRouter.cpp
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
    add_library(EnginePreviewRuntime STATIC EXCLUDE_FROM_ALL
        Preview/PreviewScene.cpp Preview/PreviewGame.cpp Preview/PreviewSession.cpp Preview/PreviewHost.cpp
        Preview/PreviewProfiler.cpp Preview/GlowBenchmark.cpp)
    target_link_libraries(EnginePreviewRuntime PUBLIC Engine EngineEditorApi EngineEditorSupport EngineProfilerRuntime)
    engine_warnings(EnginePreviewRuntime)

    function(engine_editor_escape_json result value)
        string(REPLACE "\\" "\\\\" escaped "${value}")
        string(REPLACE "\"" "\\\"" escaped "${escaped}")
        set(${result} "${escaped}" PARENT_SCOPE)
    endfunction()

    # A project supplies its preview worker (own main via RunPreviewHost); EngineEditor finds it through the
    # EngineEditor.project.json recipe generated beside both executables.
    # With no sources (a fresh project) the engine's starter worker stands in until the first source is added.
    function(engine_add_editor_worker target)
        cmake_parse_arguments(WORKER "" "NAME;ASSETS" "SOURCES;LIBRARIES" ${ARGN})
        if(NOT WORKER_NAME)
            set(WORKER_NAME "${PROJECT_NAME} previews")
        endif()
        if(NOT WORKER_SOURCES)
            message(STATUS "${target}: no preview sources yet; building the engine starter worker")
            set(WORKER_SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Preview/StarterWorker.cpp")
        endif()
        add_executable(${target} ${WORKER_SOURCES})
        target_link_libraries(${target} PRIVATE EnginePreviewRuntime ${WORKER_LIBRARIES})
        set_target_properties(${target} PROPERTIES
            CXX_EXTENSIONS OFF RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Editor/$<CONFIG>")
        engine_warnings(${target})
        if(WORKER_ASSETS)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${WORKER_ASSETS}" "$<TARGET_FILE_DIR:${target}>/Assets"
                VERBATIM)
        endif()

        engine_editor_escape_json(name "${WORKER_NAME}")
        engine_editor_escape_json(cmake "${CMAKE_COMMAND}")
        engine_editor_escape_json(build_dir "${CMAKE_BINARY_DIR}")
        file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/Editor/$<CONFIG>/EngineEditor.project.json" CONTENT
"{
  \"version\": 1,
  \"name\": \"${name}\",
  \"buildDirectory\": \"${build_dir}\",
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
    set(editor_imgui_SOURCE_DIR "${ENGINE_THIRD_PARTY_DIR}/imgui")
    set(editor_rlimgui_SOURCE_DIR "${ENGINE_THIRD_PARTY_DIR}/rlImGui")
    add_library(EngineEditorImGui STATIC
        "${editor_imgui_SOURCE_DIR}/imgui.cpp" "${editor_imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${editor_imgui_SOURCE_DIR}/imgui_tables.cpp" "${editor_imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${editor_rlimgui_SOURCE_DIR}/rlImGui.cpp")
    target_include_directories(EngineEditorImGui SYSTEM PUBLIC "${editor_imgui_SOURCE_DIR}" "${editor_rlimgui_SOURCE_DIR}")
    # The host owns font selection and icon merging, including DPI and rasterization settings.
    target_compile_definitions(EngineEditorImGui PRIVATE NO_FONT_AWESOME)
    target_link_libraries(EngineEditorImGui PRIVATE Engine)
    add_library(EngineEditorUi STATIC
        Ui/Commands/EditorCommands.cpp
        Ui/Commands/EditorCommandPalette.cpp
        Ui/Style/EditorTheme.cpp
        Ui/Widgets/EditorWidgets.cpp
        Ui/Widgets/InspectorWidgets.cpp
        Ui/Widgets/EditorPlaybackControls.cpp
        Ui/Shell/EditorShell.cpp
        Ui/Workspace/EditorWorkspace.cpp
        Ui/Panels/Catalog/CatalogPanel.cpp
        Ui/Panels/Viewport/ViewportPanel.cpp
        Ui/Panels/Inspector/InspectorPanel.cpp
        Ui/Panels/Tuning/TuningPanel.cpp
        Ui/Panels/Output/OutputPanel.cpp)
    target_sources(EngineEditorUi PRIVATE Ui/Panels/Profiler/ProfilerPanel.cpp)
    target_link_libraries(EngineEditorUi PUBLIC EngineEditorImGui EngineEditorSupport)
    engine_warnings(EngineEditorUi)
    add_executable(EngineEditor WIN32 Host/Main.cpp Host/EditorApplication.cpp Host/EditorUiContext.cpp
        Host/PreviewDisplay.cpp Application/EditorController.cpp Application/EditorProfilerCommands.cpp
        Application/EditorControlService.cpp Application/PreviewBuild.cpp Application/PreviewWorker.cpp)
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
