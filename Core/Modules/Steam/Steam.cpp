#include "Modules/Steam/Steam.h"
#include <stdexcept>
#include "Modules/Steam/Internals/SteamClient.h"
#include "Modules/Steam/Internals/SteamLaunch.h"

Steam::Steam() {
    const auto client = ReadSteamClient();
    _status.clientRunning = client.running;
    if (client.accountId) _status.account = SteamAccount{*client.accountId};
    _status.appId = ReadLaunchAppId();
    _status.launchedBySteam = _status.appId.has_value();
    if (_status.clientRunning || _status.launchedBySteam) _status.source = SteamSource::Environment;
}

void Steam::UnlockAchievement(std::string_view apiName) {
    if (!IsValidAchievementName(apiName)) throw std::invalid_argument("Invalid Steam achievement API name");
    // Without the Steamworks backend nothing is ever connected, so a valid unlock is dropped here.
}

// Steamworks backend runs callbacks and stores queued unlocks here.
void Steam::Update() {}
