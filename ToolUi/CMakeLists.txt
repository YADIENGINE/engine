# EngineToolUi: the ImGui layer shared by the engine's tools (editor, launcher): tool theme, fonts, context, window
# settings and widgets over ImGui + rlImGui. Never linked into games; built only when a tool links it.
set(tool_imgui_dir "${ENGINE_THIRD_PARTY_DIR}/imgui")
set(tool_rlimgui_dir "${ENGINE_THIRD_PARTY_DIR}/rlImGui")
# Unmodified third-party sources, kept out of the engine's warning settings.
add_library(EngineToolImGui STATIC EXCLUDE_FROM_ALL
    "${tool_imgui_dir}/imgui.cpp" "${tool_imgui_dir}/imgui_draw.cpp"
    "${tool_imgui_dir}/imgui_tables.cpp" "${tool_imgui_dir}/imgui_widgets.cpp"
    "${tool_rlimgui_dir}/rlImGui.cpp")
# Font Awesome for consumers: <extras/IconsFontAwesome6.h>.
target_include_directories(EngineToolImGui SYSTEM PUBLIC "${tool_imgui_dir}" "${tool_rlimgui_dir}")
# ToolUiContext owns font selection and icon merging, including DPI and rasterization settings.
target_compile_definitions(EngineToolImGui PRIVATE NO_FONT_AWESOME)
target_link_libraries(EngineToolImGui PRIVATE Engine)

add_library(EngineToolUi STATIC EXCLUDE_FROM_ALL Internals/WindowChrome.cpp ToolPixel.cpp ToolTheme.cpp
    ToolUiContext.cpp ToolWidgets.cpp ToolWindow.cpp)
# Embed the unmodified OFL fonts: tools look the same everywhere and do not depend on installed fonts or asset paths.
set(tool_font_header "// Generated from ThirdParty/IBMPlex; do not edit.\n")
foreach(font IN ITEMS "Sans-Regular:Regular" "Sans-Medium:Medium" "Sans-SemiBold:Heading" "Mono-Regular:Technical")
    string(REPLACE ":" ";" font_parts "${font}")
    list(GET font_parts 0 font_file_name)
    list(GET font_parts 1 font_role)
    set(font_file "${ENGINE_THIRD_PARTY_DIR}/IBMPlex/IBMPlex${font_file_name}.ttf")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${font_file}")
    file(READ "${font_file}" font_hex HEX)
    string(LENGTH "${font_hex}" font_hex_length)
    math(EXPR font_size "${font_hex_length} / 2")
    string(REGEX REPLACE "(..)" "0x\\1," font_bytes "${font_hex}")
    string(APPEND tool_font_header
           "constexpr std::array<unsigned char, ${font_size}> Tool${font_role}Data{${font_bytes}};\n")
endforeach()
set(tool_font_license "${ENGINE_THIRD_PARTY_DIR}/IBMPlex/OFL.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${tool_font_license}")
file(READ "${tool_font_license}" font_license)
# Keep the license in the executable, accessible from a tool's About/settings screen.
string(APPEND tool_font_header "constexpr const char* ToolEmbeddedFontLicense = R\"OFL(${font_license})OFL\";\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ToolFontData.h" CONTENT "${tool_font_header}")
target_include_directories(EngineToolUi PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
# Includes are relative to the engine root (#include "ToolUi/ToolTheme.h").
target_include_directories(EngineToolUi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(EngineToolUi PUBLIC EngineToolImGui Engine PRIVATE dwmapi)
engine_warnings(EngineToolUi)
