#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>

namespace arrow {

class GameAudio {
public:
    GameAudio() = default;
    ~GameAudio();
    GameAudio(const GameAudio&) = delete;
    GameAudio& operator=(const GameAudio&) = delete;

    // The audio device is opened only on the first enable. Muting stops every
    // voice immediately and keeps the decoded samples ready for re-enable.
    void setEnabled(bool enabled);
    void playArrowLaunch();
    void playCollision();
    void playDefeat();
    void playVictory();
    void playFirework();
    void playHint();
    void stop();
    void release();
    [[nodiscard]] bool ready() const noexcept;

private:
    bool load();
    void stopLaunches();
    void stopFireworks();
    void stopVoices();

    Sound launchSource_{};
    Sound collisionSource_{};
    Sound defeatSource_{};
    Sound victorySource_{};
    Sound fireworkSource_{};
    Sound hintSource_{};
    std::array<Sound, 4> launchVoices_{};
    std::array<Sound, 3> fireworkVoices_{};
    std::size_t nextVoice_{};
    std::size_t nextFirework_{};
    bool deviceOpen_{};
    bool loaded_{};
    bool enabled_{};
};

} // namespace arrow
