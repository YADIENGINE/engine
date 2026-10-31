cmake_minimum_required(VERSION 3.24)
# Proje adı: burada ve Project.json "name" alanında aynı olmalı (exe, paket ve editör worker'ı bu addan türer).
project(@PROJECT_NAME@ LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(APP_BUILD_GAME "Build the game" ON)
option(APP_BUILD_EDITOR "Build the preview worker for EngineEditor (Windows desktop)" OFF)
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

# Game/ ve Editor/ altındaki .cpp'ler kendiliğinden bulunur (yeni dosya eklenince CMake yeniden yapılandırır).
# Klasör boşken Engine'in starter oyunu / starter worker'ı derlenir.
file(GLOB_RECURSE APP_GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Game/*.cpp")
file(GLOB_RECURSE APP_EDITOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/*.cpp")

if(APP_BUILD_GAME)
    engine_add_game(${PROJECT_NAME} SOURCES ${APP_GAME_SOURCES} ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/Game/Assets")
    target_include_directories(${PROJECT_NAME} PRIVATE Game)
    # Game/ altındaki *.tuning.json dosyaları (Engine/Docs/Tuning.md); dosya yokken hiçbir şey eklemez.
    engine_add_tuning(${PROJECT_NAME} ROOT Game)
endif()

if(APP_BUILD_EDITOR)
    engine_add_editor_worker(${PROJECT_NAME}Preview SOURCES ${APP_EDITOR_SOURCES})
    target_include_directories(${PROJECT_NAME}Preview PRIVATE Editor)
endif()
