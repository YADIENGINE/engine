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

# Bağımlılıklar değiştirilmeden engine içinde durur (ThirdParty/README.md); derleme internet istemez.
set(ENGINE_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty" CACHE INTERNAL "Engine's vendored dependencies")

# Catch2'yi bir kez kurar ve catch_discover_tests'i (Catch.cmake) çağıranın kapsamına açar.
macro(engine_use_catch2)
    if(NOT TARGET Catch2::Catch2WithMain)
        set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
        set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${ENGINE_THIRD_PARTY_DIR}/Catch2" "${CMAKE_BINARY_DIR}/ThirdParty/Catch2" EXCLUDE_FROM_ALL)
    endif()
    list(APPEND CMAKE_MODULE_PATH "${ENGINE_THIRD_PARTY_DIR}/Catch2/extras")
    include(Catch)
endmacro()

# Monocypher, private to JsonIO.
add_library(Monocypher STATIC ThirdParty/Monocypher/monocypher.c)
target_include_directories(Monocypher PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Monocypher")
set_target_properties(Monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
set(JSON_SystemInclude ON CACHE BOOL "" FORCE)
add_subdirectory(ThirdParty/nlohmann_json)

# Engine kimliği tek kaynaktan: EngineManifest.json. Ad ve sürüm koda sabit yazılmaz, ENGINE_NAME / ENGINE_VERSION okunur.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/EngineManifest.json")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/EngineManifest.json" engineManifest)
string(JSON engineName GET "${engineManifest}" name)
string(JSON engineVersion GET "${engineManifest}" version)

if(ENGINE_BUILD_RUNTIME)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    function(engine_add_raylib)
        # raylib 5.5 declares a legacy minimum; apply modern compatibility only to that dependency.
        set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
        add_subdirectory(ThirdParty/raylib)
    endfunction()
    engine_add_raylib()
endif()

if(ENGINE_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(Core)
if(ENGINE_BUILD_EDITOR OR ENGINE_BUILD_EDITOR_TESTS)
    add_subdirectory(Editor)
endif()
