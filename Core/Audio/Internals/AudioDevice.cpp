#include "Audio/Internals/AudioDevice.h"
#include <raylib.h>
#include <atomic>
#include <stdexcept>

namespace {
std::atomic_flag DeviceOwned = ATOMIC_FLAG_INIT;
} // namespace

AudioDevice::AudioDevice() {
    if (DeviceOwned.test_and_set(std::memory_order_acquire))
        throw std::logic_error("The audio device is already owned");
    if (IsAudioDeviceReady()) {
        DeviceOwned.clear(std::memory_order_release);
        throw std::logic_error("The audio device is already open outside AudioSystem");
    }
}
AudioDevice::~AudioDevice() {
    if (_open) CloseAudioDevice();
    DeviceOwned.clear(std::memory_order_release);
}
void AudioDevice::Open() {
    InitAudioDevice();
    _open = IsAudioDeviceReady();
    if (!_open) throw std::runtime_error("Could not initialize the audio device");
}
