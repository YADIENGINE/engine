#pragma once
#include <array>

// Where a sound is mixed. Each bus has a level and a muffling of its own (SoundPlayer::FadeBus, MuffleBus), so that a
// pause menu can lower the world and leave its own sounds alone. The player's effects volume scales World and Ui, the
// music volume scales Music. World is what happens in the game, and its live sounds pause with the game's time; Ui is
// menus, which keep playing while the game is paused; Music is music and music-like layers.
enum class SoundBus { World, Ui, Music };
inline constexpr std::array AllSoundBuses{SoundBus::World, SoundBus::Ui, SoundBus::Music};
