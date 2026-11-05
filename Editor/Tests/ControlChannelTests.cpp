#include <catch2/catch_test_macros.hpp>
#ifdef _WIN32
#include <cstddef>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <vector>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Platform/Windows/EditorControlPipe.h"
#include "Editor/Internals/Platform/Windows/NativeHandle.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"

TEST_CASE("Editor.Channel.LatestSnapshotReplacesUnsentOne", "[engine-editor]") {
    const auto session = NewWorkerSession();
    ControlChannel server(session, true);
    ControlChannel client(session, false);
    client.SendLatest({{}, 1, 0, "inspection", {{"value", 1}}});
    client.Send({{}, 1, 7, "accepted", nlohmann::json::object()});
    client.SendLatest({{}, 1, 0, "inspection", {{"value", 2}}});
    client.SendLatest({{}, 2, 0, "inspection", {{"value", 3}}}); // Another generation keeps its own snapshot
    (void)client.Poll();
    const auto received = server.Poll();
    REQUIRE(received.size() == 3);
    CHECK(received[0].type == "accepted"); // The newer snapshot follows what was queued before it
    CHECK(received[1].data.at("value") == 2);
    CHECK(received[2].data.at("value") == 3);
}

TEST_CASE("Editor.Channel.UnreadPeerRefusesBestEffortButKeepsRoom", "[engine-editor]") {
    const auto session = NewWorkerSession();
    ControlChannel server(session, true);
    ControlChannel client(session, false); // Never polled: nothing leaves the queue
    std::size_t accepted = 0;
    for (std::size_t i = 0; i < WorkerMaxQueuedMessages; ++i)
        if (client.TrySend({{}, 0, 0, "log", {{"text", std::to_string(i)}}})) ++accepted;
    CHECK(accepted > 0);
    CHECK(accepted < WorkerMaxQueuedMessages);
    for (int i = 0; i < 100; ++i)
        client.SendLatest({{}, 0, 0, "parameters", {{"value", i}}});
    CHECK_NOTHROW(client.Send({{}, 0, 1, "accepted", nlohmann::json::object()}));
}
namespace {
NativeHandle ConnectClient(const std::string& pipe) {
    const auto name = WorkerWide(pipe);
    return NativeHandle(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
}
void Write(const NativeHandle& client, const std::string& text) {
    DWORD written = 0;
    REQUIRE(WriteFile(client.Get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr));
}
std::string ReadAvailable(const NativeHandle& client) {
    DWORD available = 0;
    if (!PeekNamedPipe(client.Get(), nullptr, 0, nullptr, &available, nullptr) || !available) return {};
    std::string text(available, ' ');
    DWORD read = 0;
    REQUIRE(ReadFile(client.Get(), text.data(), available, &read, nullptr));
    text.resize(read);
    return text;
}
} // namespace

TEST_CASE("Editor.ControlPipe.AnswersSeveralClientsAtOnce", "[engine-editor]") {
    EditorControlPipe server;
    const auto first = ConnectClient(server.Name());
    const auto second = ConnectClient(server.Name());
    REQUIRE(first.Get() != INVALID_HANDLE_VALUE);
    REQUIRE(second.Get() != INVALID_HANDLE_VALUE);
    Write(first, "one\n");
    Write(second, "two\n");
    std::vector<EditorControlPipe::Line> lines;
    for (int attempt = 0; attempt < 10 && lines.size() < 2; ++attempt)
        for (auto& line : server.Poll())
            lines.push_back(std::move(line));
    REQUIRE(lines.size() == 2);
    std::ranges::sort(lines, {}, &EditorControlPipe::Line::text);
    CHECK(lines[0].text == "one");
    CHECK(lines[1].text == "two");
    REQUIRE(lines[0].client != lines[1].client);
    CHECK(server.Connected(lines[0].client));
    // Each answer reaches only the client that asked, while the other stays connected
    server.Send(lines[1].client, "for two");
    server.Send(lines[0].client, "for one");
    (void)server.Poll();
    CHECK(ReadAvailable(first) == "for one\n");
    CHECK(ReadAvailable(second) == "for two\n");
}
#endif
