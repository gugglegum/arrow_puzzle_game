#include "AudioData.hpp"
#include "GameAudio.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
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

void waitForPlayback() {
    std::this_thread::sleep_for(std::chrono::milliseconds{350});
}

} // namespace

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_WARNING);
    arrow::GameAudio audio;
    bool attached = false;
    try {
        require(!audio.ready() && !IsAudioDeviceReady(), "Audio opened before being enabled");
        audio.playArrowLaunch();
        audio.setEnabled(false);
        require(!IsAudioDeviceReady(), "Muted launch initialized the device");
        Wave wave = LoadWaveFromMemory(".wav", arrow::assets::kArrowLaunchWav,
                                      static_cast<int>(arrow::assets::kArrowLaunchWavSize));
        require(IsWaveValid(wave) && wave.frameCount == 9120 && wave.sampleRate == 48000 &&
                wave.sampleSize == 16 && wave.channels == 1, "Embedded B2 wave is invalid");
        UnloadWave(wave);
        if (argc > 1) {
            std::ifstream source(argv[1], std::ios::binary);
            const std::vector<unsigned char> bytes{
                std::istreambuf_iterator<char>{source}, std::istreambuf_iterator<char>{}};
            require(bytes.size() == arrow::assets::kArrowLaunchWavSize &&
                    std::equal(bytes.begin(), bytes.end(), arrow::assets::kArrowLaunchWav),
                    "Embedded sound differs from the selected WAV");
        }
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
        audio.playArrowLaunch();
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
        audio.setEnabled(false);
        resetCapture();
        waitForPlayback();
        audio.playArrowLaunch();
        waitForPlayback();
        require(nonzeroSamples == 0, "Mute did not stop playback or allowed a new launch");
        require(audio.ready(), "Mute discarded the reusable audio sample");
        for (int index = 0; index < 12; ++index) {
            audio.setEnabled(true);
            audio.setEnabled(false);
        }
        audio.setEnabled(true);
        audio.playArrowLaunch();
        waitForPlayback();
        require(nonzeroSamples > 1000, "Unmute did not restore playback");
        audio.setEnabled(false);
        DetachAudioMixedProcessor(captureAndSilence);
        attached = false;
        audio.release();
        require(!audio.ready() && !IsAudioDeviceReady(), "Audio resources did not close");
        std::cout << "Audio tests passed: exact embedded B2, lazy init, overlapping voices, mute, re-enable, cleanup.\n"
                  << "Mixer peaks: single=" << singlePeak << " four=" << overlapPeak << '\n';
        return 0;
    } catch (const std::exception& error) {
        if (attached) DetachAudioMixedProcessor(captureAndSilence);
        std::cerr << "Audio test failure: " << error.what() << '\n';
        return 1;
    }
}
