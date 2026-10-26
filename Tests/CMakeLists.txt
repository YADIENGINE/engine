# Engine tests: one executable for EngineCore plus one per module (Modules/<Name>/Tests).
# Each executable links only what it tests, so a test that reaches Game or another
# module fails to build. Catch2 is declared here; consumers call FetchContent_MakeAvailable.
include(FetchContent)
set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    TIMEOUT 120
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

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
        GameOptionsTests.cpp
        GraphicsAnimationTests.cpp
        GridMenuTests.cpp
        GridViewportTests.cpp
        LifecycleTests.cpp
        MathTests.cpp
        RunStatisticsSysTests.cpp
        ShatterTests.cpp
        TextLayoutTests.cpp
        UiTransformTests.cpp
    LINKS EngineCore EngineStorage
)

# Every module with a Tests/CMakeLists.txt registers its own executable via engine_tests.
file(GLOB moduleTestLists CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Modules/*/Tests/CMakeLists.txt")
foreach(list IN LISTS moduleTestLists)
    get_filename_component(testDir "${list}" DIRECTORY)
    get_filename_component(moduleDir "${testDir}" DIRECTORY)
    get_filename_component(module "${moduleDir}" NAME)
    add_subdirectory("${testDir}" "${CMAKE_CURRENT_BINARY_DIR}/Modules/${module}")
endforeach()
