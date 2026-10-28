#include "GameAudio.hpp"
#include "AudioData.hpp"

#include <limits>

namespace arrow {

GameAudio::~GameAudio() { release(); }

bool GameAudio::load() {
    if (loaded_) return true;
    if (!deviceOpen_) {
        InitAudioDevice();
        if (!IsAudioDeviceReady()) {
            TraceLog(LOG_WARNING, "AUDIO: Sound effects unavailable; playback device did not open");
            return false;
        }
        deviceOpen_ = true;
    }
    if (assets::kArrowLaunchWavSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    Wave wave = LoadWaveFromMemory(
        ".wav", assets::kArrowLaunchWav, static_cast<int>(assets::kArrowLaunchWavSize)
    );
    if (IsWaveValid(wave)) launchSource_ = LoadSoundFromWave(wave);
    UnloadWave(wave);
    if (IsSoundValid(launchSource_)) {
        loaded_ = true;
        for (Sound& voice : launchVoices_) {
            voice = LoadSoundAlias(launchSource_);
            if (!IsSoundValid(voice)) {
                loaded_ = false;
                break;
            }
            // Four fully overlapping copies of the selected B2 remain below
            // full scale. Aliases share decoded samples, not playback cursors.
            SetSoundVolume(voice, 0.45F);
        }
        if (loaded_) return true;
    }
    TraceLog(LOG_WARNING, "AUDIO: Could not load the embedded arrow launch sound");
    release();
    return false;
}

void GameAudio::setEnabled(bool enabled) {
    if (!enabled) {
        enabled_ = false;
        stopVoices();
        return;
    }
    enabled_ = load();
}

void GameAudio::playArrowLaunch() {
    if (!enabled_ || !loaded_) return;
    for (std::size_t offset = 0; offset < launchVoices_.size(); ++offset) {
        const std::size_t index = (nextVoice_ + offset) % launchVoices_.size();
        if (!IsSoundPlaying(launchVoices_[index])) {
            PlaySound(launchVoices_[index]);
            nextVoice_ = (index + 1) % launchVoices_.size();
            return;
        }
    }
    // At extreme click rates replace the oldest, already fading launch.
    StopSound(launchVoices_[nextVoice_]);
    PlaySound(launchVoices_[nextVoice_]);
    nextVoice_ = (nextVoice_ + 1) % launchVoices_.size();
}

void GameAudio::stopVoices() {
    for (const Sound voice : launchVoices_) {
        if (voice.stream.buffer != nullptr) StopSound(voice);
    }
}

void GameAudio::release() {
    enabled_ = false;
    stopVoices();
    for (Sound& voice : launchVoices_) {
        if (voice.stream.buffer != nullptr) UnloadSoundAlias(voice);
        voice = {};
    }
    if (launchSource_.stream.buffer != nullptr) UnloadSound(launchSource_);
    launchSource_ = {};
    loaded_ = false;
    nextVoice_ = 0;
    if (deviceOpen_) CloseAudioDevice();
    deviceOpen_ = false;
}

bool GameAudio::ready() const noexcept { return loaded_; }

} // namespace arrow
