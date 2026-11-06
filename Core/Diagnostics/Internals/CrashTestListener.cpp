#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <memory>
#include "Diagnostics/Internals/CrashSession.h"

class CrashTestListener final : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;
    void testRunStarting(const Catch::TestRunInfo&) override { _session = std::make_unique<CrashSession>("test"); }
    void testCaseStarting(const Catch::TestCaseInfo& info) override { SetCrashContext(info.name); }
    // Catch2 emits testRunEnded from its SEH handler BEFORE chaining our fatal filter. Keep the installation alive
    // until listener destruction, or that chain would find its crash resources already torn down.
private:
    std::unique_ptr<CrashSession> _session;
};
CATCH_REGISTER_LISTENER(CrashTestListener)
