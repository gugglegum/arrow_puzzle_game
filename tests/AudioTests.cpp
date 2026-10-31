#include "AudioData.hpp"
#include "GameAudio.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

std::atomic<unsigned int> nonzeroSamples{};
std::atomic<float> peak{};

// Observe the game's own mixer, then silence this test's output before it
// reaches the speakers. No microphone or system-wide loopback is involved.
void captureAndSilence(void* buffer, unsigned int frames) {
    float* samples = static_cast<float*>(buffer);
    float localPeak = 0.0F;
    unsigned int nonzero = 0;
    for (unsigned int index = 0; index < frames * 2; ++index) {
        const float value = std::abs(samples[index]);
        localPeak = std::max(localPeak, value);
        nonzero += value > 0.0001F;
        samples[index] = 0.0F;
    }
    nonzeroSamples.fetch_add(nonzero);
    float previous = peak.load();
    while (previous < localPeak && !peak.compare_exchange_weak(previous, localPeak)) {}
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void resetCapture() {
    nonzeroSamples = 0;
    peak = 0.0F;
}

void waitForPlayback(int milliseconds = 350) {
    std::this_thread::sleep_for(std::chrono::milliseconds{milliseconds});
}

struct Effect {
    const char* filename;
    const unsigned char* data;
    std::size_t size;
    unsigned int frames;
    void (arrow::GameAudio::*play)();
};

} // namespace

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_WARNING);
    arrow::GameAudio audio;
    bool attached = false;
    try {
        const Effect effects[]{
            {"b2-light-pistol.wav", arrow::assets::kArrowLaunchWav,
             arrow::assets::kArrowLaunchWavSize, 9120, &arrow::GameAudio::playArrowLaunch},
            {"c3-metal-clack.wav", arrow::assets::kCollisionWav,
             arrow::assets::kCollisionWavSize, 9120, &arrow::GameAudio::playCollision},
            {"d4-retro-fail.wav", arrow::assets::kDefeatWav,
             arrow::assets::kDefeatWavSize, 45120, &arrow::GameAudio::playDefeat},
            {"v3-glass-chord.wav", arrow::assets::kVictoryWav,
             arrow::assets::kVictoryWavSize, 53760, &arrow::GameAudio::playVictory},
            {"firework-burst.wav", arrow::assets::kFireworkWav,
             arrow::assets::kFireworkWavSize, 46080, &arrow::GameAudio::playFirework},
            {"hint-chime.wav", arrow::assets::kHintWav,
             arrow::assets::kHintWavSize, 23040, &arrow::GameAudio::playHint},
        };
        require(!audio.ready() && !IsAudioDeviceReady(), "Audio opened before being enabled");
        for (const Effect& effect : effects) (audio.*effect.play)();
        audio.setEnabled(false);
        require(!IsAudioDeviceReady(), "Muted effects initialized the device");
        float maximumGameplayPeak = 0;
        for (const Effect& effect : effects) {
            Wave wave = LoadWaveFromMemory(".wav", effect.data, static_cast<int>(effect.size));
            const bool valid = IsWaveValid(wave) && wave.frameCount == effect.frames &&
                wave.sampleRate == 48000 && wave.sampleSize == 16 && wave.channels == 1;
            float* samples = valid ? LoadWaveSamples(wave) : nullptr;
            require(samples != nullptr, "Embedded wave is invalid");
            float sourcePeak = 0;
            for (unsigned int index = 0; index < wave.frameCount; ++index) {
                sourcePeak = std::max(sourcePeak, std::abs(samples[index]));
            }
            if (effect.play == &arrow::GameAudio::playArrowLaunch) {
                maximumGameplayPeak += sourcePeak * 4 * 0.45F;
            } else if (effect.play == &arrow::GameAudio::playCollision || effect.play == &arrow::GameAudio::playHint) {
                maximumGameplayPeak += sourcePeak * 0.45F;
            }
            UnloadWaveSamples(samples);
            UnloadWave(wave);
            if (argc > 1) {
                std::ifstream source(std::filesystem::path{argv[1]} / effect.filename, std::ios::binary);
                const std::vector<unsigned char> bytes{
                    std::istreambuf_iterator<char>{source}, std::istreambuf_iterator<char>{}};
                require(bytes.size() == effect.size &&
                        std::equal(bytes.begin(), bytes.end(), effect.data),
                        "Embedded sound differs from the selected WAV");
            }
        }
        // raylib 5.5's stereo mixer applies 0.6875 per channel at centre pan.
        // Sum absolute source peaks, so this also covers arbitrary start offsets.
        maximumGameplayPeak *= 0.6875F;
        require(maximumGameplayPeak < 1.0F, "Gameplay effects can exceed full scale");
        audio.setEnabled(true);
        require(audio.ready() && IsAudioDeviceReady(), "Playback device or sound is unavailable");
        AttachAudioMixedProcessor(captureAndSilence);
        attached = true;
        audio.playArrowLaunch();
        waitForPlayback();
        require(nonzeroSamples > 1000 && peak > 0.01F, "Launch did not reach the mixer");
        const float singlePeak = peak.load();
        resetCapture();
        for (int index = 0; index < 4; ++index) audio.playArrowLaunch();
        waitForPlayback();
        const float overlapPeak = peak.load();
        require(overlapPeak > singlePeak * 2.0F && overlapPeak < 1.0F,
                "Rapid launches did not overlap cleanly");
        resetCapture();
        for (int index = 0; index < 4; ++index) audio.playArrowLaunch();
        audio.playCollision();
        waitForPlayback();
        require(nonzeroSamples > 1000 && peak < 1.0F, "Combined gameplay effects clipped or stayed silent");
        const float combinedPeak = peak.load();
        for (const Effect& effect : effects) {
            resetCapture();
            (audio.*effect.play)();
            waitForPlayback(static_cast<int>(effect.frames / 48) + 150);
            require(nonzeroSamples > 1000 && peak > 0.01F && peak < 1.0F,
                    "An effect did not reach the mixer cleanly");
            (audio.*effect.play)();
            waitForPlayback(30);
            audio.setEnabled(false);
            waitForPlayback(30);
            resetCapture();
            for (const Effect& muted : effects) (audio.*muted.play)();
            waitForPlayback(150);
            require(nonzeroSamples == 0, "Mute did not stop all effects or allowed new playback");
            require(audio.ready(), "Mute discarded the reusable audio samples");
            audio.setEnabled(true);
        }
        audio.playCollision();
        audio.playDefeat();
        resetCapture();
        waitForPlayback(1100);
        require(nonzeroSamples > 1000 && peak < 1.0F, "Final impact and defeat did not mix cleanly");
        resetCapture();
        audio.playVictory();
        for (int index = 0; index < 12; ++index) audio.playFirework();
        waitForPlayback(1250);
        require(nonzeroSamples > 1000 && peak < 1.0F, "Firework voices clipped or stayed silent");
        audio.playFirework();
        waitForPlayback(30);
        audio.stop();
        waitForPlayback(30);
        resetCapture();
        waitForPlayback(150);
        require(nonzeroSamples == 0 && audio.ready(), "Pausing did not stop effects while keeping audio ready");
        for (int index = 0; index < 12; ++index) {
            audio.setEnabled(true);
            audio.setEnabled(false);
        }
        audio.setEnabled(true);
        resetCapture();
        audio.playArrowLaunch();
        waitForPlayback();
        require(nonzeroSamples > 1000, "Unmute did not restore playback");
        audio.setEnabled(false);
        DetachAudioMixedProcessor(captureAndSilence);
        attached = false;
        audio.release();
        require(!audio.ready() && !IsAudioDeviceReady(), "Audio resources did not close");
        std::cout << "Audio tests passed: six exact embedded WAVs, hints, fireworks, all effects, lazy init, overlap, mute, pause, re-enable, cleanup.\n"
                  << "Mixer peaks: single=" << singlePeak << " four=" << overlapPeak
                  << " collision+four=" << combinedPeak << " bound=" << maximumGameplayPeak << '\n';
        return 0;
    } catch (const std::exception& error) {
        if (attached) DetachAudioMixedProcessor(captureAndSilence);
        std::cerr << "Audio test failure: " << error.what() << '\n';
        return 1;
    }
}
