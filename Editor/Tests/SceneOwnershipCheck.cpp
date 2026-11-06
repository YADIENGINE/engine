// Opt-in check with a real Game (hidden window): scenes need no destructor because Game::Run shuts them down while
// they are alive, on success and on failure, and a derived game's constructor that throws leaves nothing to undo.
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Runtime/Game.h"
#include "Scene/Scene.h"

namespace {
using Journal = std::vector<std::string>;

GameSettings CheckSettings() {
    GameSettings settings;
    settings.project.applicationId = "scene-ownership-check";
    settings.project.title = "Scene ownership check";
    settings.windowVisible = false;
    settings.alwaysRun = true;
    settings.persistentOptions = false;
    return settings;
}

class JournalScene final : public Scene {
public:
    JournalScene(Journal& journal, std::string name)
        : _journal(journal)
        , _name(std::move(name)) {}
    std::function<void()> duringUpdate;

private:
    // Input cancellation is left out: the window's input repeats it. Each hook reads _name, a member of the derived
    // scene: it runs only while that member is alive.
    void Start() override { _journal.push_back(_name + ".start"); }
    void Enter() override { _journal.push_back(_name + ".enter"); }
    void Update(float) override {
        if (duringUpdate) duringUpdate();
    }
    void Exit() override { _journal.push_back(_name + ".exit"); }
    void Stop() override { _journal.push_back(_name + ".stop"); }
    Journal& _journal;
    std::string _name;
};

// Registers and selects its scenes, then fails, as a derived game's constructor might.
class ThrowingGame final : public Game {
public:
    explicit ThrowingGame(Journal& journal)
        : Game(CheckSettings())
        , _title(journal, "title")
        , _play(journal, "play") {
        RegisterScene(_title);
        RegisterScene(_play);
        SetScene(_title);
        throw std::runtime_error("derived game failed");
    }

private:
    JournalScene _title;
    JournalScene _play;
};

// Runs a few frames on one scene and then on another, then quits (or fails in the second scene's Update).
class RunningGame final : public Game {
public:
    RunningGame(Journal& journal, bool fail)
        : Game(CheckSettings())
        , _title(journal, "title")
        , _play(journal, "play") {
        if (fail) _play.duringUpdate = [] { throw std::runtime_error("scene update failed"); };
    }

private:
    void Start() override {
        RegisterScene(_title);
        RegisterScene(_play);
        SetScene(_title);
    }
    void Update(float realSeconds) override {
        Game::Update(realSeconds);
        if (++_frames == 2) SetScene(_play);
        if (_frames == 4) RequestQuit();
    }
    JournalScene _title;
    JournalScene _play;
    int _frames = 0;
};

int failures = 0;
void Expect(bool passed, const std::string& what, const Journal& journal) {
    if (passed) return;
    ++failures;
    std::cerr << "FAILED: " << what << "\n  journal:";
    for (const auto& entry : journal)
        std::cerr << ' ' << entry;
    std::cerr << '\n';
}
} // namespace

int main() {
    {
        Journal journal;
        bool thrown = false;
        try {
            ThrowingGame game(journal);
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        Expect(thrown && journal.empty(), "a throwing derived game constructor runs no scene hook", journal);
    }
    const Journal ran{"title.start", "title.enter", "title.exit", "play.start",
                      "play.enter",  "play.exit",   "play.stop",  "title.stop"};
    {
        Journal journal;
        RunningGame game(journal, false);
        game.Run();
        Expect(journal == ran, "Game::Run shuts every started scene down before it returns", journal);
    }
    {
        Journal journal;
        bool thrown = false;
        {
            RunningGame game(journal, true);
            try {
                game.Run();
            } catch (const std::runtime_error&) {
                thrown = true;
            }
        }
        Expect(thrown && journal == ran, "Game::Run shuts every started scene down before it rethrows", journal);
    }
    std::cout << "Scene ownership checks: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
