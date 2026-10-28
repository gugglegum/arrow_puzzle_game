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
    // voice immediately and keeps the small shared sample ready for re-enable.
    void setEnabled(bool enabled);
    void playArrowLaunch();
    void release();
    [[nodiscard]] bool ready() const noexcept;

private:
    bool load();
    void stopVoices();

    Sound launchSource_{};
    std::array<Sound, 4> launchVoices_{};
    std::size_t nextVoice_{};
    bool deviceOpen_{};
    bool loaded_{};
    bool enabled_{};
};

} // namespace arrow
