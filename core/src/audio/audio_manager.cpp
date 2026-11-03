// Nova Engine — silent audio manager for headless testing.
#include "nova/audio/AudioManager.hpp"
#include <map>

namespace nova {

class SilentAudioManager : public AudioManager {
public:
    bool initialize(std::string* error) override {
        (void)error;
        return true;
    }
    bool loadSound(const std::string& path, const std::string& name) override {
        (void)path;
        sounds[name] = true;
        return true;
    }
    void play(const std::string& name, float volume) override {
        (void)name; (void)volume;
        ++active;
    }
    void stop(const std::string& name) override {
        (void)name;
        if (active > 0) --active;
    }
    void setListenerPosition(float, float, float) override {}
    void setListenerOrientation(float, float, float) override {}
    int activeSources() const override { return active; }

private:
    std::map<std::string, bool> sounds;
    int active = 0;
};

std::unique_ptr<AudioManager> createSilentAudioManager() {
    return std::make_unique<SilentAudioManager>();
}

} // namespace nova
