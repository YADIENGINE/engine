cmake_minimum_required(VERSION 3.24)
# Project name: must match Project.json "name" (executable, package, and editor worker derive from this name).
project(@PROJECT_NAME@ LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(APP_BUILD_GAME "Build the game" ON)
option(APP_BUILD_EDITOR "Build the worker process for EngineEditor (Windows desktop)" OFF)
option(APP_BUILD_TESTS "Build headless tests (Debug only)" OFF)

if(APP_BUILD_GAME OR APP_BUILD_EDITOR)
    set(ENGINE_BUILD_RUNTIME ON)
else()
    set(ENGINE_BUILD_RUNTIME OFF)
endif()
set(ENGINE_BUILD_EDITOR ${APP_BUILD_EDITOR})
set(ENGINE_BUILD_TESTS ${APP_BUILD_TESTS})
if(APP_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(Engine)

# Find .cpp files under Game/ and Editor/ automatically (CMake reconfigures when a new file is added).
# When the folder is empty, prepare the Painter-based starter game / worker and its licensed font.
file(GLOB_RECURSE APP_GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Game/*.cpp")
file(GLOB_RECURSE APP_EDITOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/*.cpp")
# Work scenes are compiled once through the worker's registration-preserving OBJECT libraries.
list(FILTER APP_EDITOR_SOURCES EXCLUDE REGEX "/Editor/Work/[^/]+[.]cpp$")
# Asset registrations (X.asset.cpp) reach only the editor worker; their drawing code reaches both.
list(FILTER APP_GAME_SOURCES EXCLUDE REGEX "[.]asset[.]cpp$")
set(APP_GRAPHIC_SOURCES ${APP_GAME_SOURCES})
list(FILTER APP_GRAPHIC_SOURCES INCLUDE REGEX "/Game/Assets/Graphics/")

if(APP_BUILD_GAME)
    engine_add_game(${PROJECT_NAME} SOURCES ${APP_GAME_SOURCES} ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Assets")
    target_include_directories(${PROJECT_NAME} PRIVATE Game)
    # *.tuning.json files under Game/ (Engine/Docs/Tuning.md); adds nothing when no files exist.
    engine_add_tuning(${PROJECT_NAME} ROOT Game)
endif()

if(APP_BUILD_EDITOR)
    engine_add_editor_worker(${PROJECT_NAME}EditorWorker SOURCES ${APP_EDITOR_SOURCES} SCENE_DIRS Editor/Work
        ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Assets" GRAPHIC_SOURCES ${APP_GRAPHIC_SOURCES})
    target_include_directories(${PROJECT_NAME}EditorWorker PRIVATE Editor Game)
endif()
