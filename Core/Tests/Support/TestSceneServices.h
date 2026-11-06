#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Audio/AudioSystem.h"
#include "Audio/SoundPlayer.h"
#include "Fx/FxQuality.h"
#include "Runtime/TimeScale.h"
#include "Scene/SceneServices.h"
#include "Tests/Support/FakeSoundOutput.h"

// What Game::Services() hands a scene, without a window or a sound card: offline audio and a fake sound device.
// Link EngineAudio. `log` collects the lines a scene writes.
struct TestSceneServices {
    explicit TestSceneServices(SimulationHost* simulation = nullptr)
        : services{{1280, 720},
                   audio,
                   sounds,
                   time,
                   effects,
                   nullptr,
                   [this](const std::string& line) { log.push_back(line); },
                   simulation} {}

    AudioSystem audio{std::filesystem::path("."), {}, AudioOutput::Offline};
    FakeSoundOutput output;
    SoundPlayer sounds{{}, output};
    TimeScale time;
    FxQuality effects;
    std::vector<std::string> log;
    SceneServices services;

    TestSceneServices(const TestSceneServices&) = delete;
    TestSceneServices& operator=(const TestSceneServices&) = delete;
};
