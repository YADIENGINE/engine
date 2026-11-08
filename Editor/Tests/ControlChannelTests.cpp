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

TEST_CASE("Editor.Channel.FourMiBMessageSurvivesPartialPipeWritesAndReads", "[engine-editor]") {
    const auto session = NewWorkerSession();
    ControlChannel server(session, true);
    ControlChannel client(session, false);
    auto* sender = &server;
    auto* receiver = &client;
    SECTION("Server to client") {}
    SECTION("Client to server") {
        sender = &client;
        receiver = &server;
    }
    // Leave an unaligned prefix in the pipe so filling it also exercises a short native write.
    sender->Send({{}, 3, 6, "before-catalog", nlohmann::json::object()});
    CHECK(sender->Poll().empty());
    REQUIRE(sender->QueuedBytes() == 0);
    const std::string payload(4 * 1024 * 1024, 'x');
    sender->Send({{}, 3, 7, "catalog", {{"text", payload}}});
    sender->Send({{}, 3, 8, "after-catalog", {{"complete", true}}});
    const auto queued = sender->QueuedBytes();
    // Fill the 1 MiB pipe without reading it: a partially written message must remain queued.
    for (int i = 0; i < 128; ++i)
        CHECK(sender->Poll().empty());
    CHECK(sender->QueuedBytes() > 0);
    CHECK(sender->QueuedBytes() < queued);
    std::vector<WorkerMessage> received;
    for (int i = 0; i < 256 && received.size() < 3; ++i) {
        CHECK(sender->Poll().empty());
        for (auto& message : receiver->Poll())
            received.push_back(std::move(message));
    }
    REQUIRE(received.size() == 3);
    CHECK(received[0].type == "before-catalog");
    CHECK(received[1].type == "catalog");
    CHECK(received[1].generation == 3);
    CHECK(received[1].request == 7);
    CHECK(received[1].data.at("text") == payload);
    CHECK(received[2].type == "after-catalog");
    CHECK(received[2].request == 8);
    CHECK(sender->QueuedBytes() == 0);
}

TEST_CASE("Editor.Channel.BestEffortKeepsFourMiBCeilingAndRoomForMaximumMessage", "[engine-editor]") {
    const auto session = NewWorkerSession();
    ControlChannel server(session, true);
    ControlChannel client(session, false);
    const WorkerMessage log{{}, 0, 0, "log", {{"text", std::string(1024 * 1024, 'x')}}};
    for (int i = 0; i < 3; ++i)
        REQUIRE(client.TrySend(log));
    CHECK_FALSE(client.TrySend(log)); // Byte back-pressure, well before the message-count threshold.
    WorkerMessage maximum{session, 0, 0, "catalog", {{"text", ""}}};
    const auto envelopeBytes = EncodeWorkerMessage(maximum).size() - 4;
    maximum.data["text"] = std::string(WorkerMaxMessageBytes - envelopeBytes, 'x');
    CHECK_NOTHROW(client.Send(std::move(maximum)));
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
