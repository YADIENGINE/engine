#pragma once
#include <map>
#include <optional>
#include <string>

// Who may change or capture the editor through the control endpoint (Docs/Editor/Automation.md → Several agents).
// The editor has one scene, one playhead and one Audio Preview, so two agents taking turns would act on each other's
// state: one opens a scene, the other opens another, and the first one's screenshot shows the second's. A command that
// changes or captures (ControlCommandNeedsHold) takes the hold for its session; another session's such command is
// refused while the holder is busy or has sent one in the last IdleSeconds. After that anyone may take it.
//
// A holder that lost its hold, because the developer used the editor or another session took it while it was idle, has
// its next command refused once: the state it built may be gone, and it should look before it goes on.
//
// Clients are Console session ids (ENGINE_EDITOR_SESSION); every client outside the Console is "" and shares one hold.
// Times are seconds on one steady clock.
class ControlLease {
public:
    static constexpr double IdleSeconds = 60.0;

    // Held: another session holds the editor. Developer: the developer used the editor since this session's last
    // command. Taken: another session took the editor while this one was idle.
    enum class Refusal { Held, Developer, Taken };
    struct Answer {
        Refusal refusal = Refusal::Held;
        std::string other; // The holder (Held) or the session that took it (Taken); empty for Developer
        double idle = 0.0; // Held: seconds since the holder's last command
    };
    struct Hold {
        std::string client;
        double idle = 0.0; // Seconds since its last command; 0 while one is being answered
    };

    // Takes or renews the hold for a command that changes or captures; empty when the command may go ahead.
    [[nodiscard]] std::optional<Answer> Take(const std::string& client, double now);
    // The holder's command is still being answered, or was just answered: its idle time starts again.
    void Touch(const std::string& client, double now);
    // Gives the hold up (`ctl release`); false when `client` did not hold it.
    bool Release(const std::string& client);
    // The client's Console session ended: its hold goes without a notice to anyone.
    void Forget(const std::string& client);
    // The developer changed the editor (a scene, a parameter, playback, ...): the holder loses the hold.
    void DeveloperUsed();
    // The current hold; empty when nobody holds it or the holder has been idle for IdleSeconds.
    [[nodiscard]] std::optional<Hold> Holder(double now) const;

private:
    std::optional<std::string> _holder;
    double _last = 0.0;
    std::map<std::string, Answer> _lost; // Former holders: the notice their next command gets
};
