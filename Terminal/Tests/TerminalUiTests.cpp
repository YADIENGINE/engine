#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include "Terminal/Internals/PseudoConsole.h"
#include "Terminal/Internals/TerminalInput.h"
#include "Terminal/Internals/VtScreen.h"
#include "Terminal/TerminalSession.h"
#include "Terminal/TerminalView.h"

// Arrange a running session without a native process/window; SendWheel still encodes and flushes normally.
struct TerminalUiTestAccess {
    static void Run(TerminalSession& session) {
        session._console = std::make_unique<PseudoConsole>(PseudoConsole::Token{});
    }
    static void Feed(TerminalSession& session, std::string_view text) { session._screen->Feed(text); }
    static int Offset(const TerminalView& view) { return view._scrollOffset; }
    static TerminalPosition Anchor(const TerminalView& view) { return view._selectionAnchor; }
    static TerminalPosition End(const TerminalView& view) { return view._selectionEnd; }
};

namespace {
struct SelectionFrame {
    const std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context{ImGui::CreateContext(),
                                                                                  ImGui::DestroyContext};
    TerminalSession session;
    TerminalView view;
    ImVec2 origin;
    float cellWidth = 0;
    float cellHeight = 0;

    SelectionFrame() {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {1024, 768};
        io.DeltaTime = 1.0f / 60.0f;
        io.ConfigInputTrickleEventQueue = false;
        io.Fonts->AddFontDefaultVector();
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID(ImTextureID{1});
        TerminalUiTestAccess::Run(session);
        Frame();
        Frame();
    }
    void Frame(float seconds = 1.0f / 60.0f) {
        ImGui::GetIO().DeltaTime = seconds;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({100, 100}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({480, 240}, ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        ImGui::Begin("Terminal");
        origin = ImGui::GetCursorScreenPos();
        auto* font = ImGui::GetFontBaked();
        cellWidth = font->GetCharAdvance('M') * (ImGui::GetFontSize() / font->Size);
        cellHeight = std::round(ImGui::GetFontSize() * 1.2f);
        view.Draw(session);
        ImGui::End();
        ImGui::Render();
    }
    void Mouse(float row, float column) const {
        ImGui::GetIO().AddMousePosEvent(origin.x + column * cellWidth, origin.y + row * cellHeight);
    }
    void History() {
        for (int row = 0; row < 40; ++row)
            TerminalUiTestAccess::Feed(session, "row" + std::to_string(row) + "\r\n");
        TerminalUiTestAccess::Feed(session, "last");
        Frame();
    }
    void Drag() {
        Mouse(4.5f, 5);
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        Frame();
        Mouse(2.5f, 0);
        Frame();
    }
    std::string Copy() {
        auto& io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_C, true);
        Frame();
        const std::string text = ImGui::GetClipboardText();
        io.AddKeyEvent(ImGuiKey_C, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        Frame();
        return text;
    }
};
} // namespace

TEST_CASE("Terminal.View.WheelDuringSelectionBrowsesHistoryAndCopiesTheSelectedRows", "[terminal]") {
    SelectionFrame frame;
    frame.History();
    TerminalUiTestAccess::Feed(frame.session, "\x1b[?1000h\x1b[?1006h");
    frame.Drag();
    const auto anchor = TerminalUiTestAccess::Anchor(frame.view);
    ImGui::GetIO().AddMouseWheelEvent(0, 1);
    frame.Frame();
    CHECK_FALSE(frame.session.LastWheel());
    CHECK_FALSE(frame.session.LastInput());
    CHECK(TerminalUiTestAccess::Offset(frame.view) == 3);
    CHECK(TerminalUiTestAccess::Anchor(frame.view) == anchor);
    CHECK(TerminalUiTestAccess::End(frame.view).row == -1);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame.Frame();
    std::string expected;
    const int firstLive = 40 - frame.session.Rows() + 1;
    for (int row = firstLive - 1; row <= firstLive + 4; ++row) {
        if (!expected.empty()) expected += '\n';
        expected += "row" + std::to_string(row);
    }
    CHECK(frame.Copy() == expected);
    TerminalUiTestAccess::Feed(frame.session, "\r\nnew");
    frame.Frame();
    CHECK(frame.Copy() == expected); // Incoming output shifts selection rows relative to the live screen.
}

TEST_CASE("Terminal.View.SelectionEdgesScrollWithDistanceAndElapsedTime", "[terminal]") {
    SelectionFrame frame;
    frame.History();
    frame.Drag();
    frame.Mouse(-1, 0);
    for (int i = 0; i < 12; ++i)
        frame.Frame(1.0f / 60.0f);
    CHECK(TerminalUiTestAccess::Offset(frame.view) == 2);
    frame.Mouse(-2, 0);
    for (int i = 0; i < 24; ++i)
        frame.Frame(1.0f / 120.0f);
    CHECK(TerminalUiTestAccess::Offset(frame.view) == 6);
    frame.Mouse(static_cast<float>(frame.session.Rows()) + 1, 0);
    for (int i = 0; i < 12; ++i)
        frame.Frame(1.0f / 60.0f);
    CHECK(TerminalUiTestAccess::Offset(frame.view) == 4);
    CHECK(TerminalUiTestAccess::Anchor(frame.view) != TerminalUiTestAccess::End(frame.view));
    CHECK_FALSE(frame.session.LastWheel());
}

TEST_CASE("Terminal.View.AlternateScreenSelectionKeepsWheelLocalButNormalWheelReachesProgram", "[terminal]") {
    SelectionFrame frame;
    TerminalUiTestAccess::Feed(frame.session, "\x1b[?1049h\x1b[?1000h\x1b[?1006h");
    frame.Frame();
    frame.Drag();
    const auto anchor = TerminalUiTestAccess::Anchor(frame.view);
    const auto end = TerminalUiTestAccess::End(frame.view);
    ImGui::GetIO().AddMouseWheelEvent(0, 1);
    frame.Frame();
    CHECK(TerminalUiTestAccess::Anchor(frame.view) == anchor);
    CHECK(TerminalUiTestAccess::End(frame.view) == end);
    CHECK(TerminalUiTestAccess::Offset(frame.view) == 0);
    CHECK_FALSE(frame.session.LastWheel());
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame.Frame();
    ImGui::GetIO().AddMouseWheelEvent(0, 1);
    frame.Frame();
    CHECK(frame.session.LastWheel()); // SendWheel forwarded it; scrolling is still not typed input.
    CHECK_FALSE(frame.session.LastInput());
    CHECK(TerminalUiTestAccess::Anchor(frame.view) == TerminalUiTestAccess::End(frame.view));
}

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
