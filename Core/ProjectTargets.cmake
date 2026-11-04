# Functions a project calls to add its targets; loaded for both a source and a prebuilt engine.

# engine_add_tuning(<target> ROOT <folder>): each *.tuning.json under ROOT generates
# <build>/Tuning/<target>/<path relative to ROOT>.tuning.h at build time; that folder is a PUBLIC include directory,
# so includes match source headers. Files are found at configure time (not listed manually in CMake). See Docs/Tuning.md.
set(ENGINE_TUNING_GENERATOR_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE INTERNAL "Engine root for python -m Tools.tuning")
function(engine_add_tuning target)
    cmake_parse_arguments(PARSE_ARGV 1 TUNING "" "ROOT" "")
    if(NOT TUNING_ROOT)
        message(FATAL_ERROR "engine_add_tuning(${target}): ROOT <folder> is required")
    endif()
    cmake_path(ABSOLUTE_PATH TUNING_ROOT NORMALIZE)
    _engine_schedule_tuning_manifest()
    set_property(GLOBAL APPEND PROPERTY ENGINE_TUNING_ROOTS "${target}|${TUNING_ROOT}")
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
            "__import__('sys').path.insert(0, __import__('sys').argv.pop(1)) or __import__('runpy').run_module('Tools.tuning', run_name='__main__')"
            "${ENGINE_TUNING_GENERATOR_ROOT}" generate "${source}" "${header}" "${CMAKE_SOURCE_DIR}")
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

# The editor reads the tuning roots and files from <build>/Tuning/TuningFiles.json; written once at configure end.
function(_engine_schedule_tuning_manifest)
    get_property(scheduled GLOBAL PROPERTY ENGINE_TUNING_MANIFEST_SCHEDULED)
    if(NOT scheduled)
        set_property(GLOBAL PROPERTY ENGINE_TUNING_MANIFEST_SCHEDULED ON)
        cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _engine_write_tuning_manifest)
    endif()
endfunction()

function(_engine_tuning_manifest_list property key output)
    get_property(items GLOBAL PROPERTY ${property})
    set(entries)
    foreach(item IN LISTS items)
        string(REPLACE "|" ";" parts "${item}")
        list(GET parts 0 target)
        list(GET parts 1 path)
        string(REPLACE "\\" "\\\\" path "${path}")
        string(REPLACE "\"" "\\\"" path "${path}")
        list(APPEND entries "    { \"target\": \"${target}\", \"${key}\": \"${path}\" }")
    endforeach()
    list(JOIN entries ",\n" body)
    if(body)
        set(body "\n${body}\n  ")
    endif()
    set(${output} "${body}" PARENT_SCOPE)
endfunction()

# The editor lists every *.tuning.json under a root at once (new files show before they are built); `files` are those
# the last configure gave a header.
function(_engine_write_tuning_manifest)
    _engine_tuning_manifest_list(ENGINE_TUNING_ROOTS root roots)
    _engine_tuning_manifest_list(ENGINE_TUNING_FILES source files)
    set(project "${CMAKE_SOURCE_DIR}")
    string(REPLACE "\"" "\\\"" project "${project}")
    file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/Tuning/TuningFiles.json"
        CONTENT "{\n  \"version\": 2,\n  \"project\": \"${project}\",\n  \"roots\": [${roots}],\n  \"files\": [${files}]\n}\n" @ONLY)
endfunction()

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

# Only changed files where CMake can, so a build beside a running game does not fail on its open (streamed) assets.
function(_engine_copy_directory_command result)
    set(${result} copy_directory PARENT_SCOPE)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
        set(${result} copy_directory_if_different PARENT_SCOPE)
    endif()
endfunction()

# The engine's ready-made sounds, as Assets/Engine/Audio beside the target. `release` packages only those a game plays.
function(engine_copy_engine_audio target)
    _engine_copy_directory_command(copyDirectory)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E ${copyDirectory} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../Assets/Audio"
            "$<TARGET_FILE_DIR:${target}>/Assets/Engine/Audio"
        VERBATIM)
endfunction()

# Copies a project's asset folder beside the target. Graphics and Prefabs hold source code and stay out.
# Engine is the name of the engine's own assets there.
function(engine_copy_assets target assets)
    _engine_copy_directory_command(copyDirectory)
    set(destination "$<TARGET_FILE_DIR:${target}>/Assets")
    add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}" VERBATIM)
    file(GLOB entries CONFIGURE_DEPENDS "${assets}/*")
    foreach(entry IN LISTS entries)
        get_filename_component(name "${entry}" NAME)
        if(name STREQUAL "Engine")
            message(FATAL_ERROR "${entry}: the name Engine is reserved for the engine's assets; rename this folder")
        endif()
        if(name STREQUAL "Graphics" OR name STREQUAL "Prefabs")
            continue()
        endif()
        if(IS_DIRECTORY "${entry}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E ${copyDirectory} "${entry}" "${destination}/${name}" VERBATIM)
        else()
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${entry}" "${destination}/${name}" VERBATIM)
        endif()
    endforeach()
endfunction()

# Simulation code is always built with precise floats, so that a project flag cannot change its results silently.
# Measured: /fp:fast makes Debug and Release disagree, because Release switches to vector math (Docs/Simulation.md).
function(engine_simulation_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /fp:precise)
    endif()
endfunction()

# engine_add_simulation(<target> SOURCES <files...> [LIBRARIES <libraries...>]): a project's simulations. <target> is a
# static library for the game, the worker and tests; <target>Tool is the console tool behind `<engine> sim`, without a
# window. The sources implement RegisterSimulations (Simulation/SimulationRegistry.h).
function(engine_add_simulation target)
    cmake_parse_arguments(SIMULATION "" "" "SOURCES;LIBRARIES" ${ARGN})
    add_library(${target} STATIC ${SIMULATION_SOURCES})
    target_link_libraries(${target} PUBLIC EngineCamera EnginePhysics EngineSimulation EngineTuning
        ${SIMULATION_LIBRARIES})
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    engine_simulation_flags(${target})
    engine_warnings(${target})
    add_executable(${target}Tool "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Simulation/Internals/SimulationTool.cpp")
    target_link_libraries(${target}Tool PRIVATE ${target})
    set_target_properties(${target}Tool PROPERTIES CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
    engine_warnings(${target}Tool)
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
    add_executable(${target} ${GAME_SOURCES} "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Main.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Runtime/Internals/DevelopmentChecksBinding.cpp")
    target_link_libraries(${target} PRIVATE Engine EngineAssets ${GAME_LIBRARIES})
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    engine_warnings(${target})
    if(GAME_ASSETS)
        engine_copy_assets(${target} "${GAME_ASSETS}")
    endif()
    engine_copy_engine_audio(${target})
    if(useStarter)
        engine_copy_starter_font(${target})
    endif()
endfunction()
