#pragma once

// Reserves the process-wide backend before loading starts; closes it after all resources.
class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();
    void Open();

private:
    bool _open = false;

public:
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;
};
