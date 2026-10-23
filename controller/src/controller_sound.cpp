/**
 * @file controller_sound.cpp
 * @brief Procedural Diwali sound effects synthesizer for Phuljhari 2.1 ✨
 */

#include "controller_sound.h"
#include <cstring>

#if defined(ARDUINO)
#include <Arduino.h>
#include <M5Unified.h>
#endif

namespace phuljhari {
namespace controller {

ControllerSoundManager::ControllerSoundManager()
    : enabled_(true),
      volume_percent_(75),
      playing_(false),
      current_sfx_(SFX_NONE),
      sfx_start_ms_(0),
      sfx_duration_ms_(0),
      next_event_time_ms_(0),
      prng_state_(0x12345678),
      sub_step_(0),
      last_freq_(0),
      total_events_(0) {}

bool ControllerSoundManager::begin() {
#if defined(ARDUINO)
    uint8_t hw_vol = (uint8_t)(((uint32_t)volume_percent_ * 255U) / 100U);
    M5.Speaker.setVolume(enabled_ ? hw_vol : 0);
    // Put audio amplifier into low-power standby on startup until audio is triggered
    M5.Speaker.stop();
    M5.Speaker.end();
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
    sfx_start_ms_ = 0;
    sfx_duration_ms_ = 0;
    next_event_time_ms_ = 0;
    sub_step_ = 0;
    last_freq_ = 0;
#if defined(ARDUINO)
    M5.Speaker.stop();
    // Power down amplifier to eliminate 15-30 mA idle quiescent current
    M5.Speaker.end();
#endif
}

uint32_t ControllerSoundManager::prng_next() {
    prng_state_ ^= prng_state_ << 13;
    prng_state_ ^= prng_state_ >> 17;
    prng_state_ ^= prng_state_ << 5;
    return prng_state_;
}

uint32_t ControllerSoundManager::get_sound_elapsed_ms(uint32_t current_time_ms) const {
    if (!playing_ || current_time_ms < sfx_start_ms_) return 0;
    return (current_time_ms - sfx_start_ms_);
}

void ControllerSoundManager::play_ui_click(uint32_t current_time_ms) {
    if (!enabled_ || volume_percent_ == 0) return;
    stop();

    current_sfx_ = SFX_UI_CLICK;
    playing_ = true;
    sfx_start_ms_ = current_time_ms;
    sfx_duration_ms_ = 35;
    next_event_time_ms_ = current_time_ms;
    sub_step_ = 0;
    prng_state_ = current_time_ms ^ 0x5DEECE66D;

#if defined(ARDUINO)
    M5.Speaker.begin();
    uint8_t hw_vol = (uint8_t)(((uint32_t)volume_percent_ * 255U) / 100U);
    M5.Speaker.setVolume(hw_vol);
#endif

    generate_next_event(current_time_ms);
}

void ControllerSoundManager::play(SoundEffectId sfx, uint32_t current_time_ms) {
    if (!enabled_ || volume_percent_ == 0 || sfx == SFX_NONE) {
        stop();
        return;
    }

    // Stop active sound before launching new profile to prevent overlapping or clipping
    stop();

    current_sfx_ = sfx;
    playing_ = true;
    sfx_start_ms_ = current_time_ms;
    next_event_time_ms_ = current_time_ms;
    sub_step_ = 0;
    prng_state_ = (current_time_ms ^ (uint32_t)sfx) * 0x9E3779B9U + 1U;

    // Set duration bounds matching character celebrations
    switch (sfx) {
        case SFX_PHULJHARI:   sfx_duration_ms_ = 3500; break;
        case SFX_ANAR:        sfx_duration_ms_ = 4200; break;
        case SFX_SKY_ROCKET:  sfx_duration_ms_ = 3000; break;
        case SFX_GARLAND:     sfx_duration_ms_ = 3800; break;
        case SFX_BOMB:        sfx_duration_ms_ = 2600; break;
        case SFX_CHAKRI:      sfx_duration_ms_ = 4000; break;
        case SFX_CANDLE:      sfx_duration_ms_ = 3500; break;
        case SFX_DIYA:        sfx_duration_ms_ = 3500; break;
        case SFX_UI_CLICK:    sfx_duration_ms_ = 35;   break;
        default:
            stop();
            return;
    }

#if defined(ARDUINO)
    M5.Speaker.begin();
    uint8_t hw_vol = (uint8_t)(((uint32_t)volume_percent_ * 255U) / 100U);
    M5.Speaker.setVolume(hw_vol);
#endif

    generate_next_event(current_time_ms);
}

void ControllerSoundManager::generate_next_event(uint32_t current_time_ms) {
    if (!playing_) return;

    uint32_t elapsed = (current_time_ms >= sfx_start_ms_) ? (current_time_ms - sfx_start_ms_) : 0;
    if (elapsed >= sfx_duration_ms_) {
        stop();
        return;
    }

    uint16_t freq = 0;
    uint16_t tone_ms = 0;
    uint16_t gap_ms = 0;
    uint32_t r = prng_next();

    switch (current_sfx_) {
        /* -----------------------------------------------------------------
         * 1. Phuljhari: Continuous light sizzling with irregular tiny crackles
         * ----------------------------------------------------------------- */
        case SFX_PHULJHARI: {
            // Irregular high sparkles with randomized micro-bursts and gaps
            freq = (uint16_t)(2200 + (r % 2200));       // 2200 Hz to 4400 Hz
            tone_ms = (uint16_t)(15 + ((r >> 11) % 25)); // 15 ms to 39 ms
            gap_ms = (uint16_t)(18 + ((r >> 16) % 36));  // 18 ms to 53 ms
            break;
        }

        /* -----------------------------------------------------------------
         * 2. Anar: Rising fizz followed by dense fountain of sparkling crackles
         * ----------------------------------------------------------------- */
        case SFX_ANAR: {
            if (elapsed < 900) {
                // Stage 1: Rising fizz (ascending tone frequency ramp)
                freq = (uint16_t)(420 + (elapsed * 1400) / 900); // 420 Hz -> 1820 Hz
                tone_ms = 35;
                gap_ms = 15;
            } else if (elapsed < 3500) {
                // Stage 2: Dense sparkling fountain crackles
                freq = (uint16_t)(1600 + (r % 2200));       // 1600 Hz to 3800 Hz
                tone_ms = (uint16_t)(20 + (r % 20));        // 20 ms to 39 ms
                gap_ms = (uint16_t)(15 + ((r >> 8) % 22));  // 15 ms to 36 ms
            } else {
                // Stage 3: Gentle fade-out (crackle gaps expand)
                freq = (uint16_t)(1800 + (r % 1600));
                tone_ms = 25;
                gap_ms = (uint16_t)(40 + ((elapsed - 3500) / 10)); // 40 ms up to 110 ms
            }
            break;
        }

        /* -----------------------------------------------------------------
         * 3. Sky Rocket: Short rising whistle, launch burst, then distant pop
         * ----------------------------------------------------------------- */
        case SFX_SKY_ROCKET: {
            if (elapsed < 550) {
                // Stage 1: Rising launch whistle
                freq = (uint16_t)(650 + (elapsed * 2800) / 550); // 650 Hz -> 3450 Hz
                tone_ms = 40;
                gap_ms = 10;
            } else if (elapsed < 720) {
                // Stage 2: Launch burst puff
                freq = (uint16_t)(2400 - ((elapsed - 550) * 1100) / 170); // 2400 Hz -> 1300 Hz
                tone_ms = 45;
                gap_ms = 10;
            } else if (elapsed < 1800) {
                // Stage 3: Ascending silent travel delay as rocket climbs
                freq = 0;
                tone_ms = 0;
                gap_ms = 1800 - elapsed;
            } else if (elapsed < 1920) {
                // Stage 4: Distant sharp crack
                freq = 2400;
                tone_ms = 35;
                gap_ms = 15;
            } else if (elapsed < 2150) {
                // Stage 5: Distant warm cartoon burst pop
                freq = (uint16_t)(280 - ((elapsed - 1920) * 120) / 230); // 280 Hz -> 160 Hz
                tone_ms = 70;
                gap_ms = 30;
            } else {
                // Stage 6: Echo fade to completion
                stop();
                return;
            }
            break;
        }

        /* -----------------------------------------------------------------
         * 4. Garland Crackers: Rapid irregular popping sequence with natural fade-out
         * ----------------------------------------------------------------- */
        case SFX_GARLAND: {
            if (elapsed < 2800) {
                // Rapid irregular staccato ladi pops
                freq = (uint16_t)(680 + (r % 720));         // 680 Hz to 1400 Hz
                tone_ms = (uint16_t)(25 + (r % 18));        // 25 ms to 42 ms
                gap_ms = (uint16_t)(32 + ((r >> 8) % 45));  // 32 ms to 76 ms
            } else if (elapsed < 3500) {
                // Natural deceleration and fade-out
                freq = (uint16_t)(450 + (r % 500));         // 450 Hz to 950 Hz
                tone_ms = 30;
                gap_ms = (uint16_t)(75 + ((elapsed - 2800) / 7)); // 75 ms to 175 ms
            } else if (elapsed < 3700) {
                // Final loud satisfying pop of the ladi
                freq = 310;
                tone_ms = 70;
                gap_ms = 120;
            } else {
                stop();
                return;
            }
            break;
        }

        /* -----------------------------------------------------------------
         * 5. Patakha/Bomb: Short playful game-style pop with brief low thump
         * ----------------------------------------------------------------- */
        case SFX_BOMB: {
            if (sub_step_ == 0) {
                // Friendly game-style downward chirp
                freq = 1150;
                tone_ms = 45;
                gap_ms = 10;
                sub_step_ = 1;
            } else if (sub_step_ == 1) {
                // Playful mid pop
                freq = 600;
                tone_ms = 40;
                gap_ms = 10;
                sub_step_ = 2;
            } else if (sub_step_ == 2) {
                // Low-frequency cartoon thump
                freq = 145;
                tone_ms = 140;
                gap_ms = 50;
                sub_step_ = 3;
            } else {
                stop();
                return;
            }
            break;
        }

        /* -----------------------------------------------------------------
         * 6. Chakri: Fast rhythmic crackling with changing pitch (spinning wheel)
         * ----------------------------------------------------------------- */
        case SFX_CHAKRI: {
            // Rotational period modulates pitch continuously
            uint32_t phase = elapsed % 180; // 180 ms per revolution
            uint16_t base_freq = (phase < 90)
                                     ? (uint16_t)(620 + (phase * 680) / 90)
                                     : (uint16_t)(1300 - ((phase - 90) * 680) / 90);
            // Add procedural crackle perturbation
            int16_t jitter = (int16_t)(r % 160) - 80;
            freq = (uint16_t)(base_freq + jitter);
            tone_ms = 30;
            gap_ms = 15;
            break;
        }

        /* -----------------------------------------------------------------
         * 7. Candle: Gentle, subtle flicker or soft hiss
         * ----------------------------------------------------------------- */
        case SFX_CANDLE: {
            freq = (uint16_t)(380 + (r % 70)); // 380 Hz to 450 Hz gentle warm whisper
            tone_ms = (uint16_t)(35 + (r % 25));
            gap_ms = (uint16_t)(220 + ((r >> 8) % 180)); // 220 ms to 400 ms peaceful pause
            break;
        }

        /* -----------------------------------------------------------------
         * 8. Diya: Very quiet, warm ambient sound / serene sacred silence
         * ----------------------------------------------------------------- */
        case SFX_DIYA: {
            if (sub_step_ == 0) {
                // Single gentle sacred chime of warmth and peace
                freq = 528; // Sacred harmonious chime
                tone_ms = 130;
                gap_ms = 3400; // Then serene silence for remainder of celebration
                sub_step_ = 1;
            } else {
                stop();
                return;
            }
            break;
        }

        case SFX_UI_CLICK: {
            freq = 1400;
            tone_ms = 30;
            gap_ms = 10;
            sub_step_ = 1;
            break;
        }

        default:
            stop();
            return;
    }

    last_freq_ = freq;
    total_events_++;
    next_event_time_ms_ = current_time_ms + tone_ms + gap_ms;

#if defined(ARDUINO)
    if (freq > 0 && tone_ms > 0) {
        M5.Speaker.tone(freq, tone_ms);
    } else {
        M5.Speaker.stop();
    }
#endif
}

void ControllerSoundManager::update(uint32_t current_time_ms) {
    if (!playing_) return;

    if (current_time_ms >= next_event_time_ms_) {
        generate_next_event(current_time_ms);
    }
}

} // namespace controller
} // namespace phuljhari
