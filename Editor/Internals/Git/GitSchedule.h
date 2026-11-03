#pragma once

// When the service looks at the repository. Times are seconds on one steady clock, passed in by the caller.
class GitSchedule {
public:
    static constexpr double FetchInterval = 600;      // Fetches while the editor stays open
    static constexpr double FocusFetchInterval = 120; // Coming back to the window fetches no more often than this
    static constexpr double RefreshInterval = 4;
    static constexpr double RefreshCostFactor = 20; // A slow status (huge project) spends at most 1/20 of the time

    // An opening editor fetches right away.
    [[nodiscard]] bool FetchDue(double now) const;
    [[nodiscard]] bool RefreshDue(double now) const;
    // Seconds until something is due; the service sleeps this long unless a request arrives.
    [[nodiscard]] double WaitSeconds(double now) const;
    void FetchFinished(double now);
    // A refused sign-in does not retry by itself: repeated failures can lock an account. The developer's next action
    // (Fetch, Pull, Push) resumes it.
    void PauseFetching() { _fetchPaused = true; }
    void ResumeFetching() { _fetchPaused = false; }
    void SetFetchPossible(bool possible) { _fetchPossible = possible; }
    void RefreshFinished(double now, double seconds);
    void RefreshNow() { _refreshDirty = true; }
    void SetFocused(bool focused, double now);

private:
    double _lastFetch = -1; // Negative: none yet
    double _nextRefresh = 0;
    bool _fetchDirty = false;
    bool _fetchPaused = false;
    bool _fetchPossible = true;
    bool _refreshDirty = true;
    bool _focused = true;
};
