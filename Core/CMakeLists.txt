# Core: engine code shipped in games. This folder is the include root (#include "Runtime/Game.h").

add_library(EngineStorage STATIC
    Platform/Internals/FileStorage.cpp
    Platform/Internals/SecureRandom.cpp
    Platform/Internals/StorageError.cpp
)
target_include_directories(EngineStorage PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineStorage PUBLIC cxx_std_20)
set_target_properties(EngineStorage PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
if(WIN32)
    target_link_libraries(EngineStorage PRIVATE bcrypt)
endif()
engine_warnings(EngineStorage)

add_library(EngineJsonIO STATIC
    Modules/JsonIO/JsonIO.cpp
    Modules/JsonIO/Internals/Document.cpp
    Modules/JsonIO/Internals/JsonCodec.cpp
    Modules/JsonIO/Internals/JsonSax.cpp
    Modules/JsonIO/Internals/CryptoCodec.cpp
    Modules/JsonIO/Internals/EncryptedEnvelope.cpp
    Modules/JsonIO/Internals/GzipCodec.cpp
)
target_include_directories(EngineJsonIO PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineJsonIO PUBLIC cxx_std_20)
target_link_libraries(EngineJsonIO PRIVATE EngineStorage Monocypher nlohmann_json::nlohmann_json)
set_target_properties(EngineJsonIO PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineJsonIO)

# Opt-in, headless run archive; runtime observation is a separate optional bridge.
add_library(EngineRunTracking STATIC EXCLUDE_FROM_ALL Modules/RunTracking/RunTracking.cpp)
target_include_directories(EngineRunTracking PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineRunTracking PUBLIC cxx_std_20)
target_link_libraries(EngineRunTracking PUBLIC EngineJsonIO)
set_target_properties(EngineRunTracking PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineRunTracking)

add_library(EnginePersistence STATIC
    Modules/Persistence/Persistence.cpp
    Modules/Persistence/Internals/State.cpp
    Modules/Persistence/Internals/EncryptionKey.cpp
    Modules/Persistence/Internals/EncryptionKeyStore.cpp
    Modules/Persistence/Internals/SensitiveMemory.cpp
)
target_include_directories(EnginePersistence PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EnginePersistence PUBLIC cxx_std_20)
target_link_libraries(EnginePersistence PUBLIC EngineJsonIO PRIVATE EngineStorage)
set_target_properties(EnginePersistence PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
if(WIN32)
    target_link_libraries(EnginePersistence PRIVATE crypt32)
endif()
engine_warnings(EnginePersistence)

add_library(EngineScoreBoard STATIC
    Modules/ScoreBoard/ScoreBoard.cpp
    Modules/ScoreBoard/Internals/State.cpp
)
target_include_directories(EngineScoreBoard PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineScoreBoard PUBLIC cxx_std_20)
target_link_libraries(EngineScoreBoard PUBLIC EnginePersistence)
set_target_properties(EngineScoreBoard PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineScoreBoard)

add_library(EngineLocalization STATIC
    Modules/Localization/Localization.cpp
    Modules/Localization/Internals/LanguageInfo.cpp
)
target_include_directories(EngineLocalization PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineLocalization PUBLIC cxx_std_20)
set_target_properties(EngineLocalization PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineLocalization)

add_library(EngineHeadless STATIC
    Audio/Internals/AudioVolumePolicy.cpp
    Audio/Internals/Validation.cpp
    Fx/CameraEffects2D.cpp
    Fx/CameraShot.cpp
    Fx/FloatingText2D.cpp
    Fx/FxStage.cpp
    Fx/FxText.cpp
    Fx/GlitchEffects2D.cpp
    Fx/IrisTransition.cpp
    Fx/OverlayEffects2D.cpp
    Fx/ParticleRecipes.cpp
    Fx/ParticleSystem2D.cpp
    Fx/PulseOverlay2D.cpp
    Fx/RingEffects2D.cpp
    Fx/ShatterPattern.cpp
    Fx/ShockwaveEffects2D.cpp
    Fx/SpeedLines.cpp
    Fx/TimeScale.cpp
    Fx/TransitionClock.cpp
    GameObjects/GameObject.cpp
    GameObjects/ObjectList.cpp
    Graphics/Anim/AnimatedCounter.cpp
    Graphics/Anim/ButtonAnim.cpp
    Graphics/Draw/BackgroundDraw.cpp
    Graphics/Draw/DrawUtils.cpp
    Menus/GridViewport.cpp
    Graphics/Draw/Neon.cpp
    Graphics/Draw/ParticleDraw.cpp
    Graphics/Icons/Icons.cpp
    Graphics/Style/Theme.cpp
    Graphics/Ui/Backdrop.cpp
    Graphics/Ui/UiDraw.cpp
    Graphics/Gpu/Internals/NullDevice.cpp
    Graphics/Paint/Internals/DrawList.cpp
    Graphics/Paint/Internals/Tessellator.cpp
    Graphics/Paint/Painter.cpp
    Graphics/Paint/PainterText.cpp
    Graphics/Paint/Path.cpp
    Graphics/Render/Internals/FrameExecutor.cpp
    Graphics/Render/Internals/PostChain.cpp
    Graphics/Render/Internals/RenderPipeline.cpp
    Graphics/Render/Internals/RenderResources.cpp
    Graphics/Surface/Surface.cpp
    Graphics/Text/FontFace.cpp
    Graphics/Text/Internals/AtlasPacker.cpp
    Graphics/Text/Internals/FontData.cpp
    Graphics/Text/Internals/GlyphCache.cpp
    Graphics/Text/Internals/TextLayoutCache.cpp
    Input/Internals/CollectionPointer.cpp
    Input/Internals/DragPointer.cpp
    Input/Internals/InputNavigation.cpp
    Input/Internals/InputNormalizer.cpp
    Input/Internals/InputSample.cpp
    Input/Internals/PrimaryContact.cpp
    Menus/HeaderGridNavigation.cpp
    Menus/GridMenuSelection.cpp
    Options/GameOptions.cpp
    Platform/Internals/UserData.cpp
    Runtime/Internals/StartupTimer.cpp
    Scene/Internals/SceneManager.cpp
    Scene/Scene.cpp
    Tuning/Internals/TuningJson.cpp
    Tuning/TuningFile.cpp
    Tuning/TuningLive.cpp
    Utilities/UiTransform.cpp
    Utilities/Utf8.cpp
)
target_include_directories(EngineHeadless PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineHeadless PUBLIC cxx_std_20)
set_target_properties(EngineHeadless PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
# Tuning values are read live from files in development builds and are constexpr in Release configurations (Docs/Tuning.md).
target_compile_definitions(EngineHeadless PUBLIC ENGINE_NAME="${engineName}" ENGINE_VERSION="${engineVersion}"
    ENGINE_TUNING_LIVE=$<IF:$<CONFIG:Release,MinSizeRel,RelWithDebInfo>,0,1>)
target_link_libraries(EngineHeadless PUBLIC EnginePersistence PRIVATE EngineStorage)
# Glyph rasterization compiles a private copy of stb_truetype from raylib's bundled headers (header only, no raylib).
set_source_files_properties(Graphics/Text/Internals/FontData.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${ENGINE_THIRD_PARTY_DIR}/raylib/src/external")
engine_warnings(EngineHeadless)

# engine_add_tuning(<target> ROOT <folder>): each *.tuning.json under ROOT generates
# <build>/Tuning/<target>/<path relative to ROOT>.tuning.h at build time; that folder is a PUBLIC include directory,
# so includes match source headers. Files are found at configure time (not listed manually in CMake). See Docs/Tuning.md.
set(ENGINE_TUNING_GENERATOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE INTERNAL "Engine root for python -m Tools.tuning")
function(engine_add_tuning target)
    cmake_parse_arguments(PARSE_ARGV 1 TUNING "" "ROOT" "")
    if(NOT TUNING_ROOT)
        message(FATAL_ERROR "engine_add_tuning(${target}): ROOT <folder> is required")
    endif()
    cmake_path(ABSOLUTE_PATH TUNING_ROOT NORMALIZE)
    _engine_schedule_tuning_manifest()
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${TUNING_ROOT}/*.tuning.json")
    if(NOT sources)
        return()
    endif()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    file(GLOB generatorSources CONFIGURE_DEPENDS "${ENGINE_TUNING_GENERATOR_ROOT}/Tools/tuning/*.py")
    set(outputRoot "${CMAKE_BINARY_DIR}/Tuning/${target}")
    set(headers)
    set(structs)
    foreach(source IN LISTS sources)
        cmake_path(RELATIVE_PATH source BASE_DIRECTORY "${TUNING_ROOT}" OUTPUT_VARIABLE relative)
        string(REGEX REPLACE "\\.json$" ".h" relativeHeader "${relative}")
        cmake_path(GET source FILENAME name)
        if(name IN_LIST structs)
            message(FATAL_ERROR "${target}: two tuning files are named ${name}; each target needs unique names "
                "because the generated structs are global")
        endif()
        list(APPEND structs "${name}")
        set(header "${outputRoot}/${relativeHeader}")
        # Add the engine root to sys.path explicitly: an isolated interpreter (the launcher's bundled Python) does not
        # search the working directory with `-m`.
        set(command "${Python3_EXECUTABLE}" -B -c
            "import runpy, sys; sys.path.insert(0, sys.argv.pop(1)); runpy.run_module('Tools.tuning', run_name='__main__')"
            "${ENGINE_TUNING_GENERATOR_ROOT}" generate "${source}" "${header}")
        if(NOT EXISTS "${header}")
            # Let IntelliSense find it before the first build; the build reports any errors.
            execute_process(COMMAND ${command} WORKING_DIRECTORY "${ENGINE_TUNING_GENERATOR_ROOT}"
                OUTPUT_QUIET ERROR_QUIET)
        endif()
        add_custom_command(OUTPUT "${header}"
            COMMAND ${command}
            DEPENDS "${source}" ${generatorSources}
            WORKING_DIRECTORY "${ENGINE_TUNING_GENERATOR_ROOT}"
            COMMENT "Generating tuning header ${relativeHeader}"
            VERBATIM)
        list(APPEND headers "${header}")
        set_property(GLOBAL APPEND PROPERTY ENGINE_TUNING_FILES "${target}|${source}")
    endforeach()
    target_sources(${target} PRIVATE ${headers})
    target_include_directories(${target} PUBLIC "${outputRoot}")
endfunction()

# The editor's tuning panel reads the file list from <build>/Tuning/TuningFiles.json; written once at configure end.
function(_engine_schedule_tuning_manifest)
    get_property(scheduled GLOBAL PROPERTY ENGINE_TUNING_MANIFEST_SCHEDULED)
    if(NOT scheduled)
        set_property(GLOBAL PROPERTY ENGINE_TUNING_MANIFEST_SCHEDULED ON)
        cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _engine_write_tuning_manifest)
    endif()
endfunction()

function(_engine_write_tuning_manifest)
    get_property(files GLOBAL PROPERTY ENGINE_TUNING_FILES)
    set(entries)
    foreach(file IN LISTS files)
        string(REPLACE "|" ";" parts "${file}")
        list(GET parts 0 target)
        list(GET parts 1 source)
        string(REPLACE "\\" "\\\\" source "${source}")
        string(REPLACE "\"" "\\\"" source "${source}")
        list(APPEND entries "    { \"target\": \"${target}\", \"source\": \"${source}\" }")
    endforeach()
    list(JOIN entries ",\n" body)
    if(body)
        set(body "\n${body}\n  ")
    endif()
    file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/Tuning/TuningFiles.json"
        CONTENT "{\n  \"version\": 1,\n  \"files\": [${body}]\n}\n" @ONLY)
endfunction()

# Opt-in: headless profiler statistics and reports, independent of the runtime/editor.
add_library(EngineProfiler STATIC EXCLUDE_FROM_ALL
    Modules/Profiler/Profiler.cpp Modules/Profiler/Internals/Reports.cpp)
target_include_directories(EngineProfiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineProfiler PUBLIC cxx_std_20)
target_link_libraries(EngineProfiler PUBLIC EngineJsonIO)
set_target_properties(EngineProfiler PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineProfiler)

# Opt-in runtime bridge; normal games need not link it.
if(ENGINE_BUILD_RUNTIME)
    add_library(EngineProfilerRuntime STATIC EXCLUDE_FROM_ALL
        Modules/Profiler/Internals/RuntimeBinding.cpp Modules/Profiler/Internals/GpuTimer.cpp)
    target_link_libraries(EngineProfilerRuntime PUBLIC EngineProfiler Engine)
    if(WIN32)
        target_link_libraries(EngineProfilerRuntime PRIVATE opengl32 advapi32)
    endif()
    engine_warnings(EngineProfilerRuntime)
endif()

# Opt-in: only consumers of EngineVersion compile installed-version tracking.
add_library(EngineVersion STATIC EXCLUDE_FROM_ALL Modules/Version/Version.cpp)
target_include_directories(EngineVersion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineVersion PUBLIC cxx_std_20)
target_link_libraries(EngineVersion PUBLIC EngineHeadless)
set_target_properties(EngineVersion PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineVersion)

# Opt-in: only consumers of EngineSteam compile Steam detection.
add_library(EngineSteam STATIC EXCLUDE_FROM_ALL
    Modules/Steam/Steam.cpp
    Modules/Steam/Internals/SteamClient.cpp
    Modules/Steam/Internals/SteamLaunch.cpp
)
target_include_directories(EngineSteam PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineSteam PUBLIC cxx_std_20)
if(WIN32)
    target_link_libraries(EngineSteam PRIVATE advapi32)
endif()
set_target_properties(EngineSteam PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineSteam)

if(ENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()

if(NOT ENGINE_BUILD_RUNTIME)
    return()
endif()
add_library(Engine STATIC
    Audio/AudioSystem.cpp
    Audio/Internals/AudioDevice.cpp
    Audio/Internals/AudioMusicStream.cpp
    Audio/Internals/EffectVoices.cpp
    Audio/Internals/State.cpp
    Graphics/Gpu/Internals/GlDevice.cpp
    Graphics/Render/Internals/Renderer.cpp
    Graphics/Render/Internals/Window.cpp
    Input/Internals/InputReader.cpp
    Platform/Internals/ApplicationFiles.cpp
    Platform/Internals/Shell.cpp
    Graphics/Ui/UiButton.cpp
    Runtime/Game.cpp
)
target_link_libraries(Engine PUBLIC EngineHeadless raylib)
# GLSL sources are compiled into the engine: Graphics/Gpu/Internals/Shaders/Name.glsl -> NameShader in EngineShaders.h.
# Configure-time generation; editing a shader re-runs configure.
file(GLOB engineShaders CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Graphics/Gpu/Internals/Shaders/*.glsl")
set(engineShaderHeader "#pragma once\n#include <string_view>\n// Generated from Graphics/Gpu/Internals/Shaders; do not edit.\n")
foreach(shader IN LISTS engineShaders)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${shader}")
    get_filename_component(shaderName "${shader}" NAME_WE)
    file(READ "${shader}" shaderSource)
    string(APPEND engineShaderHeader
        "inline constexpr std::string_view ${shaderName}Shader = R\"glsl(${shaderSource})glsl\";\n")
endforeach()
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/Shaders/EngineShaders.h" CONTENT "${engineShaderHeader}" @ONLY)
target_include_directories(Engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
set_target_properties(Engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(Engine)

add_library(EngineRunTrackingRuntime STATIC EXCLUDE_FROM_ALL
    Modules/RunTracking/Internals/EngineGodAccessor.cpp
)
target_link_libraries(EngineRunTrackingRuntime PUBLIC EngineRunTracking PRIVATE Engine)
set_target_properties(EngineRunTrackingRuntime PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineRunTrackingRuntime)

# Starter placeholders use Painter text; ship their font and its license beside the executable.
function(engine_copy_starter_font target)
    set(starterFontRoot "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../ThirdParty/NotoSans")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/Assets/Fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${starterFontRoot}/NotoSans-Regular.ttf"
            "$<TARGET_FILE_DIR:${target}>/Assets/Fonts/Starter.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${starterFontRoot}/OFL.txt"
            "$<TARGET_FILE_DIR:${target}>/Assets/Fonts/Starter-OFL.txt"
        VERBATIM)
endfunction()

# A project supplies CreateGame, its source list, and an optional asset directory.
# With no sources (a fresh project) the engine's starter game stands in until the first source is added.
function(engine_add_game target)
    cmake_parse_arguments(GAME "" "ASSETS" "SOURCES;LIBRARIES" ${ARGN})
    set(useStarter FALSE)
    if(NOT GAME_SOURCES)
        message(STATUS "${target}: no game sources yet; building the engine starter game")
        set(GAME_SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Runtime/Starter/StarterGame.cpp")
        set(useStarter TRUE)
    endif()
    add_executable(${target} ${GAME_SOURCES} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Main.cpp")
    target_link_libraries(${target} PRIVATE Engine ${GAME_LIBRARIES})
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    engine_warnings(${target})
    if(GAME_ASSETS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${GAME_ASSETS}" "$<TARGET_FILE_DIR:${target}>/Assets")
    endif()
    if(useStarter)
        engine_copy_starter_font(${target})
    endif()
endfunction()
