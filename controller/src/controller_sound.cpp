/**
 * @file controller_sound.cpp
 * @brief Implementation of procedural sound synthesis for Phuljhari 2.0 ✨
 */

#include "controller_sound.h"
#include <cstring>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

namespace phuljhari {
namespace controller {

ControllerSoundManager::ControllerSoundManager()
    : enabled_(true),
      volume_percent_(75),
      playing_(false),
      current_sfx_(SFX_NONE),
      current_step_idx_(0),
      total_steps_(0),
      step_start_ms_(0) {
    std::memset(active_steps_, 0, sizeof(active_steps_));
}

bool ControllerSoundManager::begin() {
#if defined(ARDUINO)
    uint8_t hw_vol = (uint8_t)(((uint32_t)volume_percent_ * 255U) / 100U);
    M5.Speaker.setVolume(enabled_ ? hw_vol : 0);
#endif
    return true;
}

void ControllerSoundManager::set_enabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        stop();
    }
#if defined(ARDUINO)
    uint8_t hw_vol = (uint8_t)(((uint32_t)volume_percent_ * 255U) / 100U);
    M5.Speaker.setVolume(enabled_ ? hw_vol : 0);
#endif
}

void ControllerSoundManager::set_volume(uint8_t volume_percent) {
    if (volume_percent > 100) volume_percent = 100;
    volume_percent_ = volume_percent;
    if (volume_percent_ == 0) {
        stop();
    }
#if defined(ARDUINO)
    uint8_t hw_vol = (uint8_t)(((uint32_t)volume_percent_ * 255U) / 100U);
    M5.Speaker.setVolume(enabled_ ? hw_vol : 0);
#endif
}

void ControllerSoundManager::stop() {
    playing_ = false;
    current_sfx_ = SFX_NONE;
    total_steps_ = 0;
    current_step_idx_ = 0;
#if defined(ARDUINO)
    M5.Speaker.stop();
#endif
}

void ControllerSoundManager::play_ui_click(uint32_t current_time_ms) {
    if (!enabled_ || volume_percent_ == 0) return;
    total_steps_ = 1;
    active_steps_[0] = { 1400, 30 };
    current_sfx_ = SFX_UI_CLICK;
    start_step(0, current_time_ms);
}

void ControllerSoundManager::play(SoundEffectId sfx, uint32_t current_time_ms) {
    if (!enabled_ || volume_percent_ == 0 || sfx == SFX_NONE) {
        stop();
        return;
    }

    current_sfx_ = sfx;
    total_steps_ = 0;

    switch (sfx) {
        case SFX_PHULJHARI:
            // Delicate sparkling shimmer (high bright sparkles)
            total_steps_ = 5;
            active_steps_[0] = { 2400, 70 };
            active_steps_[1] = { 2900, 60 };
            active_steps_[2] = { 2200, 80 };
            active_steps_[3] = { 3200, 70 };
            active_steps_[4] = { 2600, 90 };
            break;

        case SFX_ANAR:
            // Rising fountain shimmer (ascending tones)
            total_steps_ = 5;
            active_steps_[0] = { 450, 90 };
            active_steps_[1] = { 750, 90 };
            active_steps_[2] = { 1200, 80 };
            active_steps_[3] = { 1800, 80 };
            active_steps_[4] = { 2400, 110 };
            break;

        case SFX_SKY_ROCKET:
            // Ascending whoosh / whistle followed by cartoon burst
            total_steps_ = 5;
            active_steps_[0] = { 500, 80 };
            active_steps_[1] = { 900, 80 };
            active_steps_[2] = { 1600, 90 };
            active_steps_[3] = { 2400, 100 };
            active_steps_[4] = { 180, 140 }; // Soft cartoon pop
            break;

        case SFX_GARLAND:
            // Quick rhythmic series of playful pops
            total_steps_ = 6;
            active_steps_[0] = { 850, 45 };
            active_steps_[1] = { 600, 45 };
            active_steps_[2] = { 950, 45 };
            active_steps_[3] = { 700, 45 };
            active_steps_[4] = { 1100, 50 };
            active_steps_[5] = { 500, 60 };
            break;

        case SFX_BOMB:
            // Friendly mascot drop whistle followed by playful pop
            total_steps_ = 4;
            active_steps_[0] = { 650, 70 };
            active_steps_[1] = { 450, 70 };
            active_steps_[2] = { 250, 80 };
            active_steps_[3] = { 140, 130 }; // Playful low pop
            break;

        case SFX_CHAKRI:
            // Whirling airy hum / chaser
            total_steps_ = 6;
            active_steps_[0] = { 600, 60 };
            active_steps_[1] = { 850, 60 };
            active_steps_[2] = { 1050, 60 };
            active_steps_[3] = { 750, 60 };
            active_steps_[4] = { 950, 60 };
            active_steps_[5] = { 550, 80 };
            break;

        case SFX_CANDLE_DIYA:
            // Gentle harmonic peaceful chimes
            total_steps_ = 3;
            active_steps_[0] = { 523, 120 }; // C5
            active_steps_[1] = { 659, 120 }; // E5
            active_steps_[2] = { 784, 180 }; // G5
            break;

        default:
            stop();
            return;
    }

    start_step(0, current_time_ms);
}

void ControllerSoundManager::start_step(size_t step_idx, uint32_t current_time_ms) {
    if (step_idx >= total_steps_) {
        stop();
        return;
    }

    current_step_idx_ = step_idx;
    step_start_ms_ = current_time_ms;
    playing_ = true;

#if defined(ARDUINO)
    uint8_t hw_vol = (uint8_t)(((uint32_t)volume_percent_ * 255U) / 100U);
    M5.Speaker.setVolume(hw_vol);
    M5.Speaker.tone(active_steps_[step_idx].freq_hz, active_steps_[step_idx].duration_ms);
#endif
}

void ControllerSoundManager::update(uint32_t current_time_ms) {
    if (!playing_ || total_steps_ == 0) {
        return;
    }

    while (playing_ && (current_time_ms - step_start_ms_ >= active_steps_[current_step_idx_].duration_ms)) {
        uint32_t step_dur = active_steps_[current_step_idx_].duration_ms;
        if (current_step_idx_ + 1 < total_steps_) {
            start_step(current_step_idx_ + 1, step_start_ms_ + step_dur);
        } else {
            stop();
            break;
        }
    }
}

} // namespace controller
} // namespace phuljhari
