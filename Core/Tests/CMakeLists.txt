# Engine tests: one executable for EngineHeadless plus one per module (Modules/<Name>/Tests).
# Each executable links only what it tests, so a test that reaches Game or another
# module fails to build. Game tests call engine_use_catch2() for the same Catch2.
engine_use_catch2()

# Game-free helpers shared by Engine, module and game tests.
add_library(EngineTestSupport STATIC Support/GzipInflate.cpp Support/TempDirectory.cpp)
target_include_directories(EngineTestSupport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(EngineTestSupport PUBLIC Catch2::Catch2)
engine_warnings(EngineTestSupport)

# Builds every Engine test executable; the root test preset builds it with the game tests.
add_custom_target(EngineTestSuite)

# engine_tests(<target> SOURCES <files...> LINKS <libraries...>)
function(engine_tests target)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "" "SOURCES;LINKS")
    add_executable(${target} ${TEST_SOURCES})
    target_link_libraries(${target} PRIVATE ${TEST_LINKS} EngineTestSupport Catch2::Catch2WithMain)
    engine_warnings(${target})
    add_dependencies(EngineTestSuite ${target})
    catch_discover_tests(${target}
        DISCOVERY_MODE PRE_TEST
        ADD_TAGS_AS_LABELS
        PROPERTIES TIMEOUT 30
        EXTRA_ARGS --colour-mode none --rng-seed 1729
    )
endfunction()

engine_tests(EngineTests
    SOURCES
        AudioTests.cpp
        CollectionPointerTests.cpp
        DragPointerTests.cpp
        FrameworkTests.cpp
        FxTests.cpp
        GameOptionsTests.cpp
        GraphicsAnimationTests.cpp
        GridMenuTests.cpp
        GridViewportTests.cpp
        InputTests.cpp
        LifecycleTests.cpp
        MathTests.cpp
        ObjectListSafetyTests.cpp
        PaintTests.cpp
        PaintGeometryTests.cpp
        PaintTextTests.cpp
        PaintWidgetTests.cpp
        RenderPipelineTests.cpp
        SceneManagerTests.cpp
        SceneObjectsTests.cpp
        SceneSafetyTests.cpp
        SceneTimeTests.cpp
        ShatterTests.cpp
        TuningTests.cpp
        UiTransformTests.cpp
    LINKS EngineHeadless EngineStorage
)
engine_add_tuning(EngineTests ROOT Tuning)
target_compile_definitions(EngineTests PRIVATE
    ENGINE_TUNING_TEST_FILE="${CMAKE_CURRENT_SOURCE_DIR}/Tuning/TestValues.tuning.json"
    ENGINE_TUNING_CASES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../Tools/Tests/TuningCases"
    ENGINE_TEST_FONT="${ENGINE_THIRD_PARTY_DIR}/NotoSans/NotoSans-Regular.ttf")

# Offline audio authoring (Python): one CTest per suite, label "audio-generation". Registered only when the
# interpreter has NumPy and SciPy, so configuring without them does not fail; the review player suite needs Node.
find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
set(engineRoot "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(audioGenerationRoot "${engineRoot}/Tools")
if(Python3_Interpreter_FOUND)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import numpy, scipy"
        RESULT_VARIABLE audioGenerationDependencies OUTPUT_QUIET ERROR_QUIET)
endif()
if(Python3_Interpreter_FOUND AND audioGenerationDependencies EQUAL 0)
    file(GLOB audioGenerationSuites CONFIGURE_DEPENDS "${audioGenerationRoot}/AudioGeneration/Tests/test_*.py")
    foreach(suite IN LISTS audioGenerationSuites)
        get_filename_component(suiteName "${suite}" NAME_WE)
        add_test(NAME "AudioGeneration.${suiteName}"
            COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s AudioGeneration/Tests -p "${suiteName}.py"
            WORKING_DIRECTORY "${audioGenerationRoot}")
        set_tests_properties("AudioGeneration.${suiteName}" PROPERTIES LABELS "audio-generation" TIMEOUT 300)
    endforeach()
else()
    message(STATUS "AudioGeneration tests skipped: Python 3.10+ with numpy and scipy not found")
endif()
# Internals/ boundaries (Docs/Contributing/ApiBoundaries.md) over the whole source tree: the game's when Engine is
# a submodule, Engine's own when built alone. The engine's Python tools and notice checks need no compiler either.
if(Python3_Interpreter_FOUND)
    add_test(NAME "Engine.IncludeBoundaries"
        COMMAND "${Python3_EXECUTABLE}" -B "${engineRoot}" --project "${CMAKE_SOURCE_DIR}" check)
    add_test(NAME "Engine.Tools"
        COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s Tools/Tests -t .
        WORKING_DIRECTORY "${engineRoot}")
    add_test(NAME "Engine.ThirdPartyNotices"
        COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s ThirdParty/Tests
        WORKING_DIRECTORY "${engineRoot}")
    set_tests_properties("Engine.IncludeBoundaries" "Engine.Tools" "Engine.ThirdPartyNotices"
        PROPERTIES LABELS "engine" TIMEOUT 60)
else()
    message(STATUS "Engine Python checks skipped: Python 3.10+ not found")
endif()
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME "AudioGeneration.review_player"
        COMMAND "${NODE_EXECUTABLE}" --test AudioGeneration/Tests/review_player.test.cjs
        WORKING_DIRECTORY "${audioGenerationRoot}")
    set_tests_properties("AudioGeneration.review_player" PROPERTIES LABELS "audio-generation" TIMEOUT 60)
endif()

# Every module with a Tests/CMakeLists.txt registers its own executable via engine_tests.
file(GLOB moduleTestLists CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/*/Tests/CMakeLists.txt")
foreach(list IN LISTS moduleTestLists)
    get_filename_component(testDir "${list}" DIRECTORY)
    get_filename_component(moduleDir "${testDir}" DIRECTORY)
    get_filename_component(module "${moduleDir}" NAME)
    add_subdirectory("${testDir}" "${CMAKE_CURRENT_BINARY_DIR}/Modules/${module}")
endforeach()
