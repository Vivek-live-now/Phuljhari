/**
 * @file controller_sound.h
 * @brief Procedural Diwali sound effects synthesizer for Phuljhari 2.1 ✨
 */

#ifndef PHULJHARI_CONTROLLER_SOUND_H
#define PHULJHARI_CONTROLLER_SOUND_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "character_types.h"

namespace phuljhari {
namespace controller {

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
    uint32_t get_sound_elapsed_ms(uint32_t current_time_ms) const;

    /* Testing & Telemetry Inspection */
    uint16_t get_last_freq() const { return last_freq_; }
    uint32_t get_total_events() const { return total_events_; }

private:
    void generate_next_event(uint32_t current_time_ms);
    uint32_t prng_next();

    bool enabled_;
    uint8_t volume_percent_;
    bool playing_;
    SoundEffectId current_sfx_;
    uint32_t sfx_start_ms_;
    uint32_t sfx_duration_ms_;
    uint32_t next_event_time_ms_;
    uint32_t prng_state_;
    uint16_t sub_step_;

    uint16_t last_freq_;
    uint32_t total_events_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_SOUND_H */
