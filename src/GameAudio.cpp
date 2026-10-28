#include "GameAudio.hpp"
#include "AudioData.hpp"

#include <limits>

namespace arrow {
namespace {

Sound loadEmbeddedSound(const unsigned char* data, std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    Wave wave = LoadWaveFromMemory(".wav", data, static_cast<int>(size));
    Sound sound{};
    if (IsWaveValid(wave)) sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    if (IsSoundValid(sound)) SetSoundVolume(sound, 0.45F);
    return sound;
}

} // namespace

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
    launchSource_ = loadEmbeddedSound(assets::kArrowLaunchWav, assets::kArrowLaunchWavSize);
    collisionSource_ = loadEmbeddedSound(assets::kCollisionWav, assets::kCollisionWavSize);
    defeatSource_ = loadEmbeddedSound(assets::kDefeatWav, assets::kDefeatWavSize);
    victorySource_ = loadEmbeddedSound(assets::kVictoryWav, assets::kVictoryWavSize);
    if (IsSoundValid(launchSource_) && IsSoundValid(collisionSource_) &&
        IsSoundValid(defeatSource_) && IsSoundValid(victorySource_)) {
        loaded_ = true;
        for (Sound& voice : launchVoices_) {
            voice = LoadSoundAlias(launchSource_);
            if (!IsSoundValid(voice)) {
                loaded_ = false;
                break;
            }
            // Four B2 launches plus one C3 collision stay below full scale.
            // Aliases share decoded samples, not playback cursors.
            SetSoundVolume(voice, 0.45F);
        }
        if (loaded_) return true;
    }
    TraceLog(LOG_WARNING, "AUDIO: Could not load the embedded sound effects");
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

void GameAudio::playCollision() {
    if (!enabled_ || !loaded_) return;
    // Reuse one voice for simultaneous or closely spaced life-loss batches.
    PlaySound(collisionSource_);
}

void GameAudio::playDefeat() {
    if (!enabled_ || !loaded_) return;
    stopLaunches();
    StopSound(victorySource_);
    // Preserve the final impact beneath the defeat cue.
    PlaySound(defeatSource_);
}

void GameAudio::playVictory() {
    if (!enabled_ || !loaded_) return;
    stopVoices();
    PlaySound(victorySource_);
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

void GameAudio::stopLaunches() {
    for (const Sound voice : launchVoices_) {
        if (voice.stream.buffer != nullptr) StopSound(voice);
    }
}

void GameAudio::stopVoices() {
    stopLaunches();
    for (const Sound sound : {collisionSource_, defeatSource_, victorySource_}) {
        if (sound.stream.buffer != nullptr) StopSound(sound);
    }
}

void GameAudio::release() {
    enabled_ = false;
    stopVoices();
    for (Sound& voice : launchVoices_) {
        if (voice.stream.buffer != nullptr) UnloadSoundAlias(voice);
        voice = {};
    }
    for (Sound* sound : {&launchSource_, &collisionSource_, &defeatSource_, &victorySource_}) {
        if (sound->stream.buffer != nullptr) UnloadSound(*sound);
        *sound = {};
    }
    loaded_ = false;
    nextVoice_ = 0;
    if (deviceOpen_) CloseAudioDevice();
    deviceOpen_ = false;
}

bool GameAudio::ready() const noexcept { return loaded_; }

} // namespace arrow
