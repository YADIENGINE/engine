#include "Editor/Internals/Agent/ControlLease.h"

std::optional<ControlLease::Answer> ControlLease::Take(const std::string& client, double now) {
    if (const auto lost = _lost.find(client); lost != _lost.end()) {
        auto answer = lost->second;
        _lost.erase(lost);
        return answer;
    }
    if (_holder && *_holder != client) {
        if (now - _last < IdleSeconds) return Answer{Refusal::Held, *_holder, now - _last};
        _lost[*_holder] = {Refusal::Taken, client, 0.0};
    }
    _holder = client;
    _last = now;
    return std::nullopt;
}
void ControlLease::Touch(const std::string& client, double now) {
    if (_holder == client) _last = now;
}
bool ControlLease::Release(const std::string& client) {
    if (_holder != client) return false;
    _holder.reset();
    return true;
}
void ControlLease::Forget(const std::string& client) {
    if (_holder == client) _holder.reset();
    _lost.erase(client);
}
void ControlLease::DeveloperUsed() {
    if (!_holder) return;
    _lost[*_holder] = {Refusal::Developer, {}, 0.0};
    _holder.reset();
}
std::optional<ControlLease::Hold> ControlLease::Holder(double now) const {
    if (!_holder || now - _last >= IdleSeconds) return std::nullopt;
    return Hold{*_holder, now - _last};
}
