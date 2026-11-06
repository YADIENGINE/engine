# Core: engine code shipped in games. This folder is the include root (#include "Runtime/Game.h").

include("${CMAKE_CURRENT_LIST_DIR}/ProjectTargets.cmake")

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

# Tuning values are read live from files in development builds and are constexpr in Release configurations (Docs/Tuning.md).
# Low-level so that a simulation can read them without reaching drawing or audio.
add_library(EngineTuning STATIC
    Tuning/Internals/TuningJson.cpp
    Tuning/TuningFile.cpp
    Tuning/TuningLive.cpp
)
target_include_directories(EngineTuning PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineTuning PUBLIC cxx_std_20)
target_compile_definitions(EngineTuning PUBLIC ENGINE_TUNING_LIVE=$<IF:$<CONFIG:Release,MinSizeRel,RelWithDebInfo>,0,1>)
target_link_libraries(EngineTuning PRIVATE EngineStorage)
set_target_properties(EngineTuning PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineTuning)

# Randomness (Utilities/Random): seeded numbers, weighted tables, scattered points and smooth noise over FastNoiseLite,
# which stays private. Built with precise floats like simulation code, so simulations may use all of it; everything
# above EngineSimulation gets it from there.
add_library(EngineRandom STATIC
    Utilities/Random/DrawTable.cpp
    Utilities/Random/Internals/NoiseField.cpp
    Utilities/Random/Scatter.cpp
)
target_include_directories(EngineRandom PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(EngineRandom SYSTEM PRIVATE "${ENGINE_THIRD_PARTY_DIR}/FastNoiseLite")
target_compile_features(EngineRandom PUBLIC cxx_std_20)
set_target_properties(EngineRandom PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_simulation_flags(EngineRandom)
engine_warnings(EngineRandom)

# Generated layouts (Docs/Procedural.md): rooms and corridors, random walks, caves, floor regions. Precise floats and no
# drawing, so a simulation may generate its level; engine_add_simulation links it.
add_library(EngineProcedural STATIC
    Procedural/Caves.cpp
    Procedural/RandomWalk.cpp
    Procedural/Regions.cpp
    Procedural/Rooms.cpp
)
target_include_directories(EngineProcedural PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineProcedural PUBLIC cxx_std_20)
target_link_libraries(EngineProcedural PUBLIC EngineRandom)
set_target_properties(EngineProcedural PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_simulation_flags(EngineProcedural)
engine_warnings(EngineProcedural)

# Field reflection for plain structs (Utilities/Reflection.h): PFR, header only. The one vendored library a public header
# includes, because poses and State walk a game's own structs inside templates; only Reflection.h names it.
add_library(EngineReflection INTERFACE)
target_include_directories(EngineReflection SYSTEM INTERFACE "${ENGINE_THIRD_PARTY_DIR}/pfr/include")
target_compile_features(EngineReflection INTERFACE cxx_std_20)

# Fixed-step simulation, tapes, hashes and scenarios (Docs/Simulation.md). It links nothing that draws or plays sound, so
# simulation code that reaches them fails to build.
add_library(EngineSimulation STATIC
    Simulation/InputTape.cpp
    Simulation/ReplayCheck.cpp
    Simulation/Scenario.cpp
    Simulation/ScenarioScript.cpp
    Simulation/Simulation.cpp
    Simulation/SimulationEventLog.cpp
    Simulation/SimulationRegistry.cpp
    Simulation/SimulationRunner.cpp
    Simulation/SimulationSettings.cpp
    Simulation/StateDump.cpp
    Simulation/StateHash.cpp
    Simulation/StateVisitor.cpp
)
target_include_directories(EngineSimulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineSimulation PUBLIC cxx_std_20)
target_compile_definitions(EngineSimulation PRIVATE ENGINE_VERSION="${engineVersion}")
target_link_libraries(EngineSimulation PUBLIC EngineJsonIO EngineRandom EngineReflection EngineTuning)
set_target_properties(EngineSimulation PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_simulation_flags(EngineSimulation)
engine_warnings(EngineSimulation)

# Collision queries over Box2D (Docs/Collision.md). Box2D stays private: its names reach no public header. Built with
# precise floats like simulation code, because simulations call it.
add_library(EnginePhysics STATIC
    Physics/Collision/Collision.cpp
    Physics/Collision/CollisionWorld.cpp
    Physics/Collision/Shape.cpp
    Physics/Internals/Box2dBridge.cpp
)
target_include_directories(EnginePhysics PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EnginePhysics PUBLIC cxx_std_20)
target_link_libraries(EnginePhysics PUBLIC EngineSimulation PRIVATE box2d)
set_target_properties(EnginePhysics PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_simulation_flags(EnginePhysics)
engine_simulation_flags(box2d)
engine_warnings(EnginePhysics)

# Game cameras (Docs/Camera.md): follow, framing, limits, split screen. Built with precise floats like simulation code,
# because a simulation may keep a camera that decides gameplay. It draws nothing; its debug guides are in
# EngineHeadless.
add_library(EngineCamera STATIC
    Camera/Camera2D.cpp
    Camera/Cameras.cpp
    Camera/CameraSettings.cpp
    Camera/Internals/CameraSolver.cpp
)
target_include_directories(EngineCamera PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineCamera PUBLIC cxx_std_20)
target_link_libraries(EngineCamera PUBLIC EngineReflection)
set_target_properties(EngineCamera PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_simulation_flags(EngineCamera)
engine_warnings(EngineCamera)

# Polygon geometry for Paint, private to EngineHeadless: earcut triangulates (header only), Clipper2 offsets and
# resolves fill rules. Vendored and unmodified, so their warnings stay off. Clipper2's triangulation is not built.
add_library(EngineEarcut INTERFACE)
target_include_directories(EngineEarcut SYSTEM INTERFACE "${ENGINE_THIRD_PARTY_DIR}/earcut/include")
set(clipper2Dir "${ENGINE_THIRD_PARTY_DIR}/Clipper2/CPP/Clipper2Lib")
add_library(EngineClipper2 STATIC
    "${clipper2Dir}/src/clipper.engine.cpp"
    "${clipper2Dir}/src/clipper.offset.cpp"
    "${clipper2Dir}/src/clipper.rectclip.cpp"
)
target_include_directories(EngineClipper2 SYSTEM PUBLIC "${clipper2Dir}/include")
target_compile_features(EngineClipper2 PUBLIC cxx_std_17)
set_target_properties(EngineClipper2 PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(EngineClipper2 PRIVATE /W0)
endif()

add_library(EngineHeadless STATIC
    Audio/Internals/AudioVolumePolicy.cpp
    Audio/Internals/LiveVoices.cpp
    Audio/Internals/Validation.cpp
    Audio/LiveSound.cpp
    Audio/SoundLayers.cpp
    Audio/SoundPlayer.cpp
    Audio/SoundTables.cpp
    Camera/Debug/CameraGuides.cpp
    Diagnostics/DevelopmentChecks.cpp
    Fx/Camera/CameraEffects.cpp
    Fx/Camera/CameraShot.cpp
    Fx/FxCue.cpp
    Fx/FxCueSet.cpp
    Fx/FxStage.cpp
    Fx/FxText.cpp
    Fx/Particles/ParticleEmitter.cpp
    Fx/Particles/ParticleRecipes.cpp
    Fx/Particles/ParticleSystem.cpp
    Fx/Screen/Announcements.cpp
    Fx/Screen/PulseOverlay.cpp
    Fx/Screen/ScreenFlash.cpp
    Fx/Screen/ScreenGlitch.cpp
    Fx/Screen/SlowMotionOverlay.cpp
    Fx/World/FloatingTexts.cpp
    Fx/World/Rings.cpp
    Fx/World/ShatterSpread.cpp
    Fx/World/Bolts.cpp
    Fx/World/FlashLights.cpp
    Fx/World/Shockwaves.cpp
    GameObjects/GameObject.cpp
    GameObjects/ObjectList.cpp
    Graphics/Draw/BackgroundDraw.cpp
    Graphics/Draw/DrawUtils.cpp
    Graphics/Draw/IrisTransition.cpp
    Graphics/Draw/Neon.cpp
    Graphics/Draw/ParticleDraw.cpp
    Graphics/Draw/SpeedLines.cpp
    Graphics/Style/Theme.cpp
    Graphics/Gpu/Internals/NullDevice.cpp
    Graphics/Paint/Internals/DrawList.cpp
    Graphics/Paint/Internals/ShapeArea.cpp
    Graphics/Paint/Internals/Tessellator.cpp
    Graphics/Paint/Internals/Triangulator.cpp
    Graphics/Atmosphere/Atmosphere.cpp
    Graphics/Atmosphere/DriftField.cpp
    Graphics/Atmosphere/FogField.cpp
    Graphics/Atmosphere/GroundRipples.cpp
    Graphics/Light/Debug/LightGuides.cpp
    Graphics/Light/CollisionOccluders.cpp
    Graphics/Light/LightLayer.cpp
    Graphics/Light/LightOccluders.cpp
    Graphics/Paint/Painter.cpp
    Graphics/Paint/PainterText.cpp
    Graphics/Paint/Path.cpp
    Graphics/Post/PostSettings.cpp
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
    Graphics/Text/Internals/TextShaper.cpp
    Input/Internals/CollectionPointer.cpp
    Input/Internals/DragPointer.cpp
    Input/Internals/InputBindings.cpp
    Input/Internals/InputNavigation.cpp
    Input/Internals/InputNormalizer.cpp
    Input/Internals/InputSample.cpp
    Physics/Debug/ColliderDraw.cpp
    Input/Internals/PrimaryContact.cpp
    Options/GameOptions.cpp
    Platform/Internals/UserData.cpp
    Runtime/TimeScale.cpp
    Runtime/Internals/GameAssetManifest.cpp
    Runtime/Internals/StartupTimer.cpp
    Scene/Internals/SceneManager.cpp
    Scene/Scene.cpp
    Scene/TransitionClock.cpp
    Ui/Kernel/Internals/UiFocusSearch.cpp
    Ui/Kernel/Internals/UiKernel.cpp
    Ui/Kernel/Ui.cpp
    Ui/Kernel/UiElement.cpp
    Ui/Kernel/UiKeys.cpp
    Ui/Kernel/UiLayerScope.cpp
    Ui/Kernel/UiListener.cpp
    Ui/Kernel/UiLooks.cpp
    Ui/Layout/Internals/UiLayoutSolver.cpp
    Ui/Layout/UiTransform.cpp
    Ui/Looks/Neon/NeonLooks.cpp
    Ui/Kit/UiOptionsScreen.cpp
    Ui/Looks/UiButtonLook.cpp
    Ui/Looks/UiControlLook.cpp
    Ui/Looks/UiLabelLook.cpp
    Ui/Looks/UiLayerLook.cpp
    Ui/Looks/UiPanelLook.cpp
    Ui/Looks/UiScrollLook.cpp
    Ui/Looks/UiSliderLook.cpp
    Ui/Looks/UiStepperLook.cpp
    Ui/Looks/UiTabLook.cpp
    Ui/Looks/UiToggleLook.cpp
    Ui/Looks/UiTooltipLook.cpp
    Ui/Sound/UiSoundListener.cpp
    Ui/Sound/UiSounds.cpp
    Ui/Widgets/Internals/UiControlRow.cpp
    Ui/Widgets/UiButton.cpp
    Ui/Widgets/UiLabel.cpp
    Ui/Widgets/UiLayer.cpp
    Ui/Widgets/UiPanel.cpp
    Ui/Widgets/UiScroll.cpp
    Ui/Widgets/UiSlider.cpp
    Ui/Widgets/UiStepper.cpp
    Ui/Widgets/UiTabs.cpp
    Ui/Widgets/UiToggle.cpp
    Ui/Widgets/UiTooltip.cpp
    Utilities/Geometry/ShatterPattern.cpp
    Utilities/Geometry/Visibility.cpp
    Utilities/Utf8.cpp
)
target_include_directories(EngineHeadless PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineHeadless PUBLIC cxx_std_20)
set_target_properties(EngineHeadless PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
target_compile_definitions(EngineHeadless PUBLIC ENGINE_NAME="${engineName}" ENGINE_VERSION="${engineVersion}")
target_link_libraries(EngineHeadless PUBLIC EngineCamera EnginePersistence EnginePhysics EngineProcedural EngineSimulation
    EngineTuning PRIVATE EngineClipper2 EngineEarcut EngineStorage EngineTextShape)
# The engine's ready-made graphics (Assets/Graphics). They sit above the engine's systems, so nothing in Core calls them.
# X.asset.cpp registrations are left out: they reach only the editor worker.
file(GLOB_RECURSE engineAssetSources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Assets/Graphics/*.cpp")
list(FILTER engineAssetSources EXCLUDE REGEX "[.]asset[.]cpp$")
add_library(EngineAssets STATIC ${engineAssetSources})
target_include_directories(EngineAssets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../Assets")
target_link_libraries(EngineAssets PUBLIC EngineHeadless)
set_target_properties(EngineAssets PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineAssets)
# Glyph rasterization compiles a private copy of stb_truetype from raylib's bundled headers (header only, no raylib).
set_source_files_properties(Graphics/Text/Internals/FontData.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${ENGINE_THIRD_PARTY_DIR}/raylib/src/external")
engine_warnings(EngineHeadless)

# Text shaping for Graphics/Text: kb_text_shape (one C header) segments Unicode text and applies OpenType rules. Private to
# EngineHeadless; vendored and unmodified, so its warnings stay off.
add_library(EngineTextShape STATIC Graphics/Text/Internals/TextShapeLibrary.c)
target_include_directories(EngineTextShape SYSTEM PUBLIC "${ENGINE_THIRD_PARTY_DIR}/kb")
set_target_properties(EngineTextShape PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(EngineTextShape PRIVATE /W0)
endif()

# miniaudio and stb_vorbis as one object, with their warnings off: they are vendored and unmodified.
add_library(EngineMiniaudio STATIC Audio/Internals/Miniaudio.c)
target_include_directories(EngineMiniaudio SYSTEM PUBLIC "${ENGINE_THIRD_PARTY_DIR}/miniaudio")
set_target_properties(EngineMiniaudio PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(EngineMiniaudio PRIVATE /W0)
endif()

# The audio engine. It needs no window, so the real thing runs offline in the tests (Docs/Audio.md).
add_library(EngineAudio STATIC
    Audio/AudioFiles.cpp
    Audio/AudioSystem.cpp
    Audio/Internals/AudioDecoder.cpp
    Audio/Internals/AudioGraph.cpp
    Audio/Internals/AudioShaper.cpp
    Audio/Internals/AudioVoice.cpp
    Audio/Internals/ShapeNode.cpp
    Audio/Internals/State.cpp
)
target_link_libraries(EngineAudio PUBLIC EngineHeadless PRIVATE EngineMiniaudio)
set_target_properties(EngineAudio PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineAudio)

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

# Opt-in animated content; no core system depends on these presentation modules.
add_library(EnginePopup STATIC EXCLUDE_FROM_ALL
    Modules/Popup/Popup.cpp
    Modules/Popup/Internals/State.cpp
)
target_include_directories(EnginePopup PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EnginePopup PUBLIC cxx_std_20)
target_link_libraries(EnginePopup PUBLIC EngineHeadless)
set_target_properties(EnginePopup PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EnginePopup)

add_library(EngineNumberPopup STATIC EXCLUDE_FROM_ALL
    Modules/NumberPopup/NumberPopup.cpp
    Modules/NumberPopup/Internals/State.cpp
    Modules/NumberPopup/Internals/Formatting.cpp
)
target_include_directories(EngineNumberPopup PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineNumberPopup PUBLIC cxx_std_20)
target_link_libraries(EngineNumberPopup PUBLIC EnginePopup)
set_target_properties(EngineNumberPopup PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_warnings(EngineNumberPopup)

# Opt-in sight queries and drawing contours; low-level and precise so simulations may explicitly link it.
add_library(EngineVisionCone STATIC EXCLUDE_FROM_ALL
    Modules/VisionCone/VisionCone.cpp
    Modules/VisionCone/Internals/ConeGeometry.cpp
    Modules/VisionCone/Internals/SightQuery.cpp
)
target_include_directories(EngineVisionCone PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(EngineVisionCone PUBLIC cxx_std_20)
target_link_libraries(EngineVisionCone PUBLIC EnginePhysics)
set_target_properties(EngineVisionCone PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_EXTENSIONS OFF)
engine_simulation_flags(EngineVisionCone)
engine_warnings(EngineVisionCone)

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

# The engine's own small simulation: its tests, and `python . sim` in this repository.
if(ENGINE_BUILD_TESTS OR ENGINE_BUILD_TOY_SIMULATION)
    engine_add_simulation(EngineToySimulation SOURCES
        Tests/Simulation/ToyAnimation.cpp
        Tests/Simulation/ToyCollision.cpp
        Tests/Simulation/ToyProcedural.cpp
        Tests/Simulation/ToyRegistry.cpp
        Tests/Simulation/ToySimulation.cpp
        Tests/Simulation/ToyTimeline.cpp
    )
endif()
if(ENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()

if(NOT ENGINE_BUILD_RUNTIME)
    return()
endif()
add_library(Engine STATIC
    Graphics/Gpu/Internals/GlDevice.cpp
    Graphics/Render/Internals/Renderer.cpp
    Graphics/Render/Internals/Window.cpp
    Input/Internals/InputReader.cpp
    Platform/Internals/ApplicationFiles.cpp
    Platform/Internals/Shell.cpp
    Runtime/Game.cpp
)
target_link_libraries(Engine PUBLIC EngineAudio EngineHeadless raylib)
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
