# Functions a project calls to add its editor worker; loaded for both a source and a prebuilt engine.

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
    get_filename_component(engineRoot "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(engineRootBinding "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Worker/EngineRootBinding.cpp")
    add_executable(${target} ${WORKER_SOURCES} "${engineRootBinding}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../Core/Runtime/Internals/DevelopmentChecksBinding.cpp")
    set_source_files_properties("${engineRootBinding}" TARGET_DIRECTORY ${target} PROPERTIES
        COMPILE_DEFINITIONS "ENGINE_WORKER_ENGINE_ROOT=\"${engineRoot}\"")
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
