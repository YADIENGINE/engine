cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Engine LANGUAGES C CXX)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ENGINE_BUILD_RUNTIME "Build the raylib runtime" ON)
option(ENGINE_BUILD_TESTS "Build headless Engine and module tests (desktop only)" OFF)
option(ENGINE_BUILD_EDITOR "Build the standalone Windows engine editor" OFF)
option(ENGINE_BUILD_EDITOR_TESTS "Build opt-in headless editor infrastructure tests" OFF)

function(engine_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(CMAKE_GENERATOR MATCHES "Visual Studio") # Ninja zaten dosya başına paralel derler
            target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/MP>")
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Pinned, unmodified Monocypher 4.0.3, private to JsonIO; upstream license is shipped beside the sources.
add_library(Monocypher STATIC Modules/JsonIO/Internals/Monocypher/monocypher.c)
target_include_directories(Monocypher PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Modules/JsonIO/Internals/Monocypher")
set_target_properties(Monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)

include(FetchContent)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_SystemInclude ON CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(EngineStorage STATIC Platform/FileStorage.cpp Platform/StorageError.cpp Platform/SecureRandom.cpp)
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
if(ANDROID)
    target_sources(EnginePersistence PRIVATE Modules/Persistence/Internals/AndroidSaveKeys.cpp)
    target_include_directories(EnginePersistence PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")
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

add_library(EngineCore STATIC
    Audio/Internals/AudioVolumePolicy.cpp
    Audio/Internals/Validation.cpp
    Fx/ShatterPattern.cpp
    Fx/TimeScale.cpp
    Fx/TransitionClock.cpp
    Graphics/Anim/AnimatedCounter.cpp
    Graphics/Anim/ButtonAnim.cpp
    Graphics/Draw/EffectQuality.cpp
    Graphics/Draw/GridViewport.cpp
    Graphics/Draw/TextLayout.cpp
    Input/CollectionPointer.cpp
    Input/DragPointer.cpp
    Menus/HeaderGridNavigation.cpp
    Menus/GridMenuSelection.cpp
    Options/GameOptions.cpp
    Tracking/RunStatisticsSys.cpp
    Platform/UserData.cpp
    Runtime/GameObject.cpp
    Runtime/ObjectList.cpp
    Runtime/Scene.cpp
    Utilities/StartupTimer.cpp
    Utilities/UiTransform.cpp
)
target_include_directories(EngineCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineCore PUBLIC cxx_std_20)
set_target_properties(EngineCore PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
target_link_libraries(EngineCore PUBLIC EnginePersistence PRIVATE EngineStorage)
engine_warnings(EngineCore)

# Opt-in: only consumers of EngineVersion compile installed-version tracking.
add_library(EngineVersion STATIC EXCLUDE_FROM_ALL Modules/Version/Version.cpp)
target_include_directories(EngineVersion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineVersion PUBLIC cxx_std_20)
target_link_libraries(EngineVersion PUBLIC EngineCore)
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
    enable_testing()
    add_subdirectory(Tests)
endif()

if(ENGINE_BUILD_EDITOR OR ENGINE_BUILD_EDITOR_TESTS)
    add_subdirectory(Editor)
endif()

if(NOT ENGINE_BUILD_RUNTIME)
    return()
endif()
if(ANDROID)
    set(PLATFORM Android CACHE STRING "raylib target platform" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib statically" FORCE)
endif()
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
function(engine_fetch_raylib)
    # raylib 5.5 declares a legacy minimum; apply modern compatibility only to that dependency.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    FetchContent_MakeAvailable(raylib)
endfunction()
engine_fetch_raylib()

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
    Graphics/Render/Canvas.cpp
    Graphics/Draw/DrawUtils.cpp
    Graphics/Draw/DrawTime.cpp
    Graphics/Draw/ParticleDraw.cpp
    Graphics/Draw/Neon.cpp
    Graphics/Render/RenderLayer.cpp
    Graphics/Render/RenderTargetStack.cpp
    Graphics/Render/ShadowMask.cpp
    Graphics/Render/Renderer.cpp
    Graphics/Draw/TextRenderer.cpp
    Graphics/Render/UiRegion.cpp
    Graphics/Render/Window.cpp
    Input/InputReader.cpp
    Platform/Platform.cpp
    Graphics/Style/Theme.cpp
    Graphics/Ui/Backdrop.cpp
    Graphics/Ui/UiButton.cpp
    Graphics/Ui/UiDraw.cpp
    Runtime/Game.cpp
    Runtime/EngineRunStatistics.cpp
)
target_link_libraries(Engine PUBLIC EngineCore raylib)
set_target_properties(Engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
if(ANDROID)
    target_include_directories(Engine PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")
endif()
engine_warnings(Engine)

# A project supplies CreateGame, its source list, and an optional asset directory.
# With no sources (a fresh project) the engine's starter game stands in until the first source is added.
function(engine_add_game target)
    cmake_parse_arguments(GAME "" "ASSETS" "SOURCES;LIBRARIES" ${ARGN})
    if(NOT GAME_SOURCES)
        message(STATUS "${target}: no game sources yet; building the engine starter game")
        set(GAME_SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Runtime/Starter/StarterGame.cpp")
    endif()
    if(ANDROID)
        add_library(${target} SHARED ${GAME_SOURCES} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Main.cpp")
        target_link_options(${target} PRIVATE "-Wl,-u,ANativeActivity_onCreate")
    else()
        add_executable(${target} ${GAME_SOURCES} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Main.cpp")
    endif()
    target_link_libraries(${target} PRIVATE Engine ${GAME_LIBRARIES})
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    engine_warnings(${target})
    if(GAME_ASSETS AND NOT ANDROID)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${GAME_ASSETS}" "$<TARGET_FILE_DIR:${target}>/Assets")
    endif()
endfunction()
