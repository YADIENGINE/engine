#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include "Terminal/Internals/TerminalInput.h"
#include "Terminal/TerminalSession.h"
#include "Terminal/TerminalView.h"

TEST_CASE("Terminal.Input.AltGrProducesTextWhileLeftAltAndCtrlKeepTheirMeaning", "[terminal]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    auto& rightAlt = io.KeysData[ImGuiKey_RightAlt - ImGuiKey_NamedKey_BEGIN];
    // GLFW reports AltGr as RightAlt without the synthetic Ctrl event.
    rightAlt.Down = true;
    io.KeyAlt = true;
    CHECK_FALSE(ReadTerminalModifiers().alt);
    CHECK_FALSE(ReadTerminalModifiers().ctrl);
    // Backends that keep Ctrl+Alt must produce the same unmodified text.
    io.KeyCtrl = true;
    CHECK_FALSE(ReadTerminalModifiers().alt);
    CHECK_FALSE(ReadTerminalModifiers().ctrl);
    rightAlt.Down = false;
    io.KeyCtrl = false;
    CHECK(ReadTerminalModifiers().alt);
    io.KeyAlt = false;
    io.KeyCtrl = true;
    CHECK(ReadTerminalModifiers().ctrl);
    CHECK_FALSE(ReadTerminalModifiers().alt);
}

TEST_CASE("Terminal.View.FocusedGridKeepsCharacterCaptureWithoutAnActiveWidget", "[terminal]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1024, 768};
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefaultVector();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    TerminalSession session;
    TerminalView view;
    const auto frame = [&](bool focusTerminal, bool focusOther) {
        ImGui::NewFrame();
        if (focusOther) ImGui::SetNextWindowFocus();
        ImGui::Begin("Other");
        ImGui::TextUnformatted("Another panel");
        ImGui::End();
        ImGui::SetNextWindowSize({480, 240}, ImGuiCond_Always);
        if (focusTerminal) ImGui::SetNextWindowFocus();
        ImGui::Begin("Terminal");
        view.Draw(session);
        ImGui::End();
        ImGui::Render();
    };
    frame(true, false);
    CHECK(view.Focused());
    // rlImGui checks this flag before NewFrame, even when no mouse button is held.
    CHECK(io.WantCaptureKeyboard);
    frame(false, false);
    CHECK(view.Focused());
    CHECK(io.WantCaptureKeyboard);
    frame(false, true);
    CHECK_FALSE(view.Focused());
    frame(false, false);
    CHECK_FALSE(io.WantCaptureKeyboard);
}

TEST_CASE("Terminal.View.GridPreservesFractionalFontAdvances", "[terminal]") {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context(ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1024, 768};
    io.DeltaTime = 1.0f / 60.0f;
    ImFontConfig config;
    config.PixelSnapH = false;
    config.GlyphMinAdvanceX = config.GlyphMaxAdvanceX = 7.25f;
    io.FontDefault = io.Fonts->AddFontDefaultVector(&config);
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID{1});
    TerminalSession session;
    TerminalView view;
    ImGui::NewFrame();
    ImGui::SetNextWindowSize({480, 240}, ImGuiCond_Always);
    ImGui::Begin("Terminal");
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize);
    auto* font = ImGui::GetFontBaked();
    const float advance = font->GetCharAdvance('M') * (ImGui::GetFontSize() / font->Size);
    REQUIRE(advance != std::ceil(advance));
    view.Draw(session);
    // Sizing must agree with text advances even where whole-pixel cells would fit fewer columns.
    CHECK(session.Columns() == static_cast<int>(available / advance));
    CHECK(session.Columns() > static_cast<int>(available / std::ceil(advance)));
    ImGui::End();
    ImGui::Render();
}
