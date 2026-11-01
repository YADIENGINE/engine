#include <catch2/catch_test_macros.hpp>
#ifdef _WIN32
#include <cstddef>
#include <string>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
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
#endif
