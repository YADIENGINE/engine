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

add_library(EngineToolUi STATIC EXCLUDE_FROM_ALL ToolTheme.cpp ToolUiContext.cpp ToolWidgets.cpp ToolWindow.cpp)
# Includes are relative to the engine root (#include "ToolUi/ToolTheme.h").
target_include_directories(EngineToolUi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(EngineToolUi PUBLIC EngineToolImGui Engine)
engine_warnings(EngineToolUi)
