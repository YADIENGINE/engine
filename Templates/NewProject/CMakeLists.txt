cmake_minimum_required(VERSION 3.24)
project(MyProject LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MYPROJECT_BUILD_GAME "Build the game" ON)
option(MYPROJECT_BUILD_EDITOR "Build the preview worker for EngineEditor (Windows desktop)" OFF)
option(MYPROJECT_BUILD_TESTS "Build headless tests (Debug only)" OFF)

if(MYPROJECT_BUILD_GAME OR MYPROJECT_BUILD_EDITOR)
    set(ENGINE_BUILD_RUNTIME ON)
else()
    set(ENGINE_BUILD_RUNTIME OFF)
endif()
set(ENGINE_BUILD_EDITOR ${MYPROJECT_BUILD_EDITOR})
set(ENGINE_BUILD_TESTS ${MYPROJECT_BUILD_TESTS})
if(MYPROJECT_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(Engine)

if(MYPROJECT_BUILD_GAME)
    engine_add_game(MyProject SOURCES Game/CreateGame.cpp Game/MyGame.cpp Game/MyScene.cpp
        ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Assets")
    target_include_directories(MyProject PRIVATE Game)
endif()

if(MYPROJECT_BUILD_EDITOR)
    engine_add_editor_worker(MyProjectPreview NAME "MyProject previews"
        SOURCES Editor/Main.cpp Editor/FirstPreview.cpp)
    target_include_directories(MyProjectPreview PRIVATE Editor)
endif()
