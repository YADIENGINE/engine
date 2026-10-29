#pragma once
#include <chrono>
#include <string>

// Açılış ölçümü: kapsamın süresini ve ilk ölçümden beri geçen süreyi "STARTUP:" önekiyle stdout'a yazar.
class StartupTimer {
public:
    explicit StartupTimer(std::string stage);
    ~StartupTimer();
    // Kapsamsız an (ör. ilk kare): sadece ilk ölçümden beri geçen süre.
    static void Mark(const char* moment);

private:
    std::string _stage;
    std::chrono::steady_clock::time_point _start;

public:
    StartupTimer(const StartupTimer&) = delete;
    StartupTimer& operator=(const StartupTimer&) = delete;
    StartupTimer(StartupTimer&&) = delete;
    StartupTimer& operator=(StartupTimer&&) = delete;
};
