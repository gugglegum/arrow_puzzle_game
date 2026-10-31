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
    fireworkSource_ = loadEmbeddedSound(assets::kFireworkWav, assets::kFireworkWavSize);
    hintSource_ = loadEmbeddedSound(assets::kHintWav, assets::kHintWavSize);
    if (IsSoundValid(launchSource_) && IsSoundValid(collisionSource_) &&
        IsSoundValid(defeatSource_) && IsSoundValid(victorySource_) && IsSoundValid(fireworkSource_) &&
        IsSoundValid(hintSource_)) {
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
        if (loaded_) {
            for (Sound& voice : fireworkVoices_) {
                voice = LoadSoundAlias(fireworkSource_);
                if (!IsSoundValid(voice)) {
                    loaded_ = false;
                    break;
                }
                SetSoundVolume(voice, 0.45F);
            }
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
    stopFireworks();
    // Preserve the final impact beneath the defeat cue.
    PlaySound(defeatSource_);
}

void GameAudio::playVictory() {
    nextFirework_ = 0;
    if (!enabled_ || !loaded_) return;
    stopVoices();
    PlaySound(victorySource_);
}

void GameAudio::playFirework() {
    const std::size_t index = nextFirework_++;
    if (!enabled_ || !loaded_) return;
    constexpr std::array<float, 5> pitches{1.0F, 0.94F, 1.06F, 0.98F, 1.04F};
    constexpr std::array<float, 5> pans{0.65F, 0.35F, 0.5F, 0.6F, 0.4F};
    const Sound voice = fireworkVoices_[index % fireworkVoices_.size()];
    StopSound(voice);
    SetSoundPitch(voice, pitches[index % pitches.size()]);
    SetSoundPan(voice, pans[index % pans.size()]);
    PlaySound(voice);
}

void GameAudio::stop() { stopVoices(); }

void GameAudio::playHint() {
    if (!enabled_ || !loaded_) return;
    StopSound(hintSource_);
    PlaySound(hintSource_);
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
    stopFireworks();
    for (const Sound sound : {collisionSource_, defeatSource_, victorySource_, hintSource_}) {
        if (sound.stream.buffer != nullptr) StopSound(sound);
    }
}

void GameAudio::stopFireworks() {
    for (const Sound voice : fireworkVoices_) {
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
    for (Sound& voice : fireworkVoices_) {
        if (voice.stream.buffer != nullptr) UnloadSoundAlias(voice);
        voice = {};
    }
    for (Sound* sound : {&launchSource_, &collisionSource_, &defeatSource_, &victorySource_, &fireworkSource_, &hintSource_}) {
        if (sound->stream.buffer != nullptr) UnloadSound(*sound);
        *sound = {};
    }
    loaded_ = false;
    nextVoice_ = 0;
    nextFirework_ = 0;
    if (deviceOpen_) CloseAudioDevice();
    deviceOpen_ = false;
}

bool GameAudio::ready() const noexcept { return loaded_; }

} // namespace arrow
