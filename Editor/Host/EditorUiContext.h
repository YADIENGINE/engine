#pragma once
#include <imgui.h>

class EditorUiContext {
public:
    EditorUiContext();
    ~EditorUiContext();
    void BeginFrame(int fontSize);
    void EndFrame() const;

private:
    ImGuiStyle _baseStyle;
    float _scale = 0;

public:
    EditorUiContext(const EditorUiContext&) = delete;
    EditorUiContext& operator=(const EditorUiContext&) = delete;
};
