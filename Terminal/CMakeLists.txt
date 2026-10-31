# EngineTerminal: a program in a Windows pseudo console (ConPTY) and its screen (libvterm) for the tools' console
# panel; EngineTerminalUi draws it with ImGui. Never linked into games; built only when a tool or test links it.
set(vterm_dir "${ENGINE_THIRD_PARTY_DIR}/libvterm")
# Unmodified third-party C sources, kept out of the engine's warning settings.
add_library(EngineVterm STATIC EXCLUDE_FROM_ALL
    "${vterm_dir}/src/encoding.c" "${vterm_dir}/src/keyboard.c" "${vterm_dir}/src/mouse.c"
    "${vterm_dir}/src/parser.c" "${vterm_dir}/src/pen.c" "${vterm_dir}/src/screen.c" "${vterm_dir}/src/state.c"
    "${vterm_dir}/src/unicode.c" "${vterm_dir}/src/vterm.c")
set_target_properties(EngineVterm PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
target_include_directories(EngineVterm SYSTEM PUBLIC "${vterm_dir}/include")
target_include_directories(EngineVterm PRIVATE "${vterm_dir}/src")
if(MSVC)
    target_compile_options(EngineVterm PRIVATE /w /utf-8)
endif()

add_library(EngineTerminal STATIC EXCLUDE_FROM_ALL
    TerminalSession.cpp Internals/PseudoConsole.cpp Internals/VtScreen.cpp)
# Includes are relative to the engine root (#include "Terminal/TerminalSession.h").
target_include_directories(EngineTerminal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(EngineTerminal PRIVATE EngineVterm)
target_compile_definitions(EngineTerminal PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
engine_warnings(EngineTerminal)

if(TARGET EngineToolUi)
    add_library(EngineTerminalUi STATIC EXCLUDE_FROM_ALL TerminalView.cpp)
    target_link_libraries(EngineTerminalUi PUBLIC EngineTerminal EngineToolUi)
    engine_warnings(EngineTerminalUi)
endif()

if(ENGINE_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
