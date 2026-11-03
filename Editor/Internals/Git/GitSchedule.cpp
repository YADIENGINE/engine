#include "Editor/Internals/Git/GitSchedule.h"
#include <algorithm>

bool GitSchedule::FetchDue(double now) const {
    if (_fetchPaused || !_fetchPossible) return false;
    return _lastFetch < 0 || _fetchDirty || now - _lastFetch >= FetchInterval;
}
bool GitSchedule::RefreshDue(double now) const { return _refreshDirty || (_focused && now >= _nextRefresh); }
double GitSchedule::WaitSeconds(double now) const {
    if (RefreshDue(now) || FetchDue(now)) return 0;
    double wait = FetchInterval;
    if (_focused) wait = std::min(wait, _nextRefresh - now);
    if (!_fetchPaused && _fetchPossible) wait = std::min(wait, _lastFetch + FetchInterval - now);
    return std::max(wait, 0.0);
}
void GitSchedule::FetchFinished(double now) {
    _lastFetch = now;
    _fetchDirty = false;
}
void GitSchedule::RefreshFinished(double now, double seconds) {
    _refreshDirty = false;
    _nextRefresh = now + std::max(RefreshInterval, seconds * RefreshCostFactor);
}
void GitSchedule::SetFocused(bool focused, double now) {
    if (focused && !_focused) {
        _refreshDirty = true;
        if (_lastFetch >= 0 && now - _lastFetch >= FocusFetchInterval) _fetchDirty = true;
    }
    _focused = focused;
}
