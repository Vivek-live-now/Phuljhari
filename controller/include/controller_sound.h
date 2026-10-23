/**
 * @file controller_sound.h
 * @brief Reusable sound manager and procedural synthesizer for Phuljhari 2.0 ✨
 */

#ifndef PHULJHARI_CONTROLLER_SOUND_H
#define PHULJHARI_CONTROLLER_SOUND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "character_types.h"

namespace phuljhari {
namespace controller {

struct ToneStep {
    uint16_t freq_hz;
    uint16_t duration_ms;
};

class ControllerSoundManager {
public:
    ControllerSoundManager();

    bool begin();
    void update(uint32_t current_time_ms);

    void set_enabled(bool enabled);
    bool is_enabled() const { return enabled_; }

    void set_volume(uint8_t volume_percent);
    uint8_t get_volume() const { return volume_percent_; }

    void play(SoundEffectId sfx, uint32_t current_time_ms);
    void play_ui_click(uint32_t current_time_ms);
    void stop();

    bool is_playing() const { return playing_; }
    SoundEffectId get_current_sound() const { return current_sfx_; }

private:
    void start_step(size_t step_idx, uint32_t current_time_ms);

    bool enabled_;
    uint8_t volume_percent_;
    bool playing_;
    SoundEffectId current_sfx_;
    size_t current_step_idx_;
    size_t total_steps_;
    uint32_t step_start_ms_;

    static const size_t MAX_STEPS = 8;
    ToneStep active_steps_[MAX_STEPS];
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_SOUND_H */
