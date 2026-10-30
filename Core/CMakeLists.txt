# Core: oyuna giren engine kodu. Include kökü bu klasördür (#include "Runtime/Game.h").

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
    Fx/ShatterPattern.cpp
    Fx/TimeScale.cpp
    Fx/TransitionClock.cpp
    GameObjects/GameObject.cpp
    GameObjects/ObjectList.cpp
    Graphics/Anim/AnimatedCounter.cpp
    Graphics/Anim/ButtonAnim.cpp
    Graphics/Draw/EffectQuality.cpp
    Graphics/Draw/GridViewport.cpp
    Graphics/Draw/TextLayout.cpp
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
    Utilities/UiTransform.cpp
)
target_include_directories(EngineHeadless PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineHeadless PUBLIC cxx_std_20)
set_target_properties(EngineHeadless PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
target_compile_definitions(EngineHeadless PUBLIC ENGINE_NAME="${engineName}" ENGINE_VERSION="${engineVersion}")
target_link_libraries(EngineHeadless PUBLIC EnginePersistence PRIVATE EngineStorage)
engine_warnings(EngineHeadless)

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
    Fx/CameraEffects2D.cpp
    Fx/CameraShot.cpp
    Fx/FxStage.cpp
    Fx/GlitchEffects2D.cpp
    Fx/ShockwaveEffects2D.cpp
    Fx/PulseOverlay2D.cpp
    Fx/FloatingText2D.cpp
    Fx/OverlayEffects2D.cpp
    Fx/ParticleSystem2D.cpp
    Fx/ParticleRecipes.cpp
    Fx/RingEffects2D.cpp
    Fx/SpeedLines.cpp
    Fx/IrisTransition.cpp
    Graphics/Icons/Icons.cpp
    Graphics/Draw/BackgroundDraw.cpp
    Graphics/Render/Internals/Canvas.cpp
    Graphics/Draw/DrawUtils.cpp
    Graphics/Draw/DrawTime.cpp
    Graphics/Draw/GlowBatch.cpp
    Graphics/Draw/ParticleDraw.cpp
    Graphics/Draw/Neon.cpp
    Graphics/Render/RenderLayer.cpp
    Graphics/Render/Internals/RenderTargetStack.cpp
    Graphics/Render/Internals/RenderExecutionContext.cpp
    Graphics/Render/ShadowMask.cpp
    Graphics/Render/Internals/Renderer.cpp
    Graphics/Draw/TextRenderer.cpp
    Graphics/Render/UiRegion.cpp
    Graphics/Render/Internals/Window.cpp
    Input/Internals/InputReader.cpp
    Platform/Internals/ApplicationFiles.cpp
    Graphics/Style/Theme.cpp
    Graphics/Ui/Backdrop.cpp
    Graphics/Ui/UiButton.cpp
    Graphics/Ui/UiDraw.cpp
    Runtime/Game.cpp
)
target_link_libraries(Engine PUBLIC EngineHeadless raylib)
set_target_properties(Engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(Engine)

add_library(EngineRunTrackingRuntime STATIC EXCLUDE_FROM_ALL
    Modules/RunTracking/Internals/EngineGodAccessor.cpp
)
target_link_libraries(EngineRunTrackingRuntime PUBLIC EngineRunTracking PRIVATE Engine)
set_target_properties(EngineRunTrackingRuntime PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineRunTrackingRuntime)

# A project supplies CreateGame, its source list, and an optional asset directory.
# With no sources (a fresh project) the engine's starter game stands in until the first source is added.
function(engine_add_game target)
    cmake_parse_arguments(GAME "" "ASSETS" "SOURCES;LIBRARIES" ${ARGN})
    if(NOT GAME_SOURCES)
        message(STATUS "${target}: no game sources yet; building the engine starter game")
        set(GAME_SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Runtime/Starter/StarterGame.cpp")
    endif()
    add_executable(${target} ${GAME_SOURCES} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Main.cpp")
    target_link_libraries(${target} PRIVATE Engine ${GAME_LIBRARIES})
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    engine_warnings(${target})
    if(GAME_ASSETS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${GAME_ASSETS}" "$<TARGET_FILE_DIR:${target}>/Assets")
    endif()
endfunction()
