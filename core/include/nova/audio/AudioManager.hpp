// Nova Engine — audio interface (2D/3D). The Android backend implements
// playback via MediaPlayer/AudioTrack; the core keeps the API abstract so
// tests can use a silent sink.
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace nova {

class AudioManager {
public:
    virtual ~AudioManager() = default;
    virtual bool initialize(std::string* error) = 0;
    virtual bool loadSound(const std::string& path, const std::string& name) = 0;
    virtual void play(const std::string& name, float volume = 1.0f) = 0;
    virtual void stop(const std::string& name) = 0;
    virtual void setListenerPosition(float x, float y, float z) = 0;
    virtual void setListenerOrientation(float forwardX, float forwardY, float upY) = 0;
    virtual int activeSources() const = 0;
};

std::unique_ptr<AudioManager> createSilentAudioManager();

} // namespace nova
