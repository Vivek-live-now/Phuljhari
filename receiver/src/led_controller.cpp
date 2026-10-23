/**
 * @file led_controller.cpp
 * @brief Implementation of FastLED decorative lighting engine for Project Phuljhari Receiver ✨
 */

#include "led_controller.h"

#include <Arduino.h>
#define FASTLED_INTERNAL
#include <FastLED.h>
#include <cstring>

namespace phuljhari {
namespace receiver {

#if PHULJHARI_ENABLE_STRIP
static CRGB s_strip_leds[PHULJHARI_CONFIG_STRIP_NUM_LEDS];
#endif

LedController::LedController()
    : last_frame_ms_(0),
      animation_step_(0),
      last_burst_ms_(0),
      burst_center_(0.0f),
      burst_radius_(0.0f),
      burst_hue_(0),
      burst_active_(false) {
    for (size_t i = 0; i < MAX_FOUNTAIN_PARTICLES; ++i) {
        fountain_particles_[i].position = 0.0f;
        fountain_particles_[i].velocity = 0.0f;
        fountain_particles_[i].hue = 0;
        fountain_particles_[i].active = false;
    }
}

void LedController::begin() {
    Serial.println("[LED] Initializing outputs with safe INACTIVE defaults...");

    // 1. Initialize Onboard Bench-Test LED
#if PHULJHARI_ENABLE_ONBOARD_LED
    pinMode(PHULJHARI_CONFIG_ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(
        PHULJHARI_CONFIG_ONBOARD_LED_PIN,
        PHULJHARI_CONFIG_ONBOARD_ACTIVE_HIGH ? LOW : HIGH
    );
    Serial.printf("[LED] Bench-test indicator configured on GPIO %d\n", PHULJHARI_CONFIG_ONBOARD_LED_PIN);
#endif

    // 2. Initialize Addressable LED Strip (FastLED)
#if PHULJHARI_ENABLE_STRIP
    FastLED.addLeds<WS2812B, PHULJHARI_CONFIG_STRIP_PIN, PHULJHARI_CONFIG_STRIP_COLOR_ORDER>(
        s_strip_leds,
        PHULJHARI_CONFIG_STRIP_NUM_LEDS
    );
    FastLED.setBrightness(0);
    FastLED.clear(true);
    Serial.printf("[LED] WS2812B strip initialized on GPIO %d with %d LEDs (Max Brightness: %d)\n",
                  PHULJHARI_CONFIG_STRIP_PIN,
                  PHULJHARI_CONFIG_STRIP_NUM_LEDS,
                  PHULJHARI_SAFETY_MAX_LED_BRIGHTNESS);
#endif

    set_all_inactive();
}

void LedController::set_all_inactive() {
#if PHULJHARI_ENABLE_STRIP
    FastLED.clear(true);
#endif

#if PHULJHARI_ENABLE_ONBOARD_LED
    digitalWrite(
        PHULJHARI_CONFIG_ONBOARD_LED_PIN,
        PHULJHARI_CONFIG_ONBOARD_ACTIVE_HIGH ? LOW : HIGH
    );
#endif
}

void LedController::update(
    protocol::ReceiverState state,
    protocol::EffectId effect,
    uint8_t intensity_pct,
    bool button_indicator_active,
    uint32_t current_time_ms
) {
    // 1. Update Bench-test Onboard LED (Strictly button-controlled)
    update_onboard_led(button_indicator_active);

    // 2. Frame-rate limiter for WS2812B strip rendering (~50 FPS / 20 ms)
    if (current_time_ms - last_frame_ms_ < PHULJHARI_CONFIG_FRAME_INTERVAL_MS) {
        return;
    }
    last_frame_ms_ = current_time_ms;
    animation_step_++;

#if PHULJHARI_ENABLE_STRIP
    // Calculate safe brightness strictly capped to prevent power brownouts
    uint8_t safe_brightness = safety::intensity_to_safe_brightness(intensity_pct);

    switch (state) {
        case protocol::RECV_STATE_EFFECT_ACTIVE:
            FastLED.setBrightness(safe_brightness);
            switch (effect) {
                case protocol::EFFECT_ID_PHULJHARI:
                    render_phuljhari(safe_brightness);
                    break;
                case protocol::EFFECT_ID_ANAR:
                    render_anar(safe_brightness);
                    break;
                case protocol::EFFECT_ID_PATAKHA:
                    render_patakha(safe_brightness);
                    break;
                default:
                    render_safe_idle();
                    break;
            }
            break;

        case protocol::RECV_STATE_COOLDOWN:
        case protocol::RECV_STATE_SAFE_IDLE:
        default:
            render_safe_idle();
            break;
    }

    FastLED.show();
#endif
}

/* -------------------------------------------------------------------------
 * Effect 1: Phuljhari ✨ (Warm Golden Sparkles & Gentle Twinkling)
 * ------------------------------------------------------------------------- */
void LedController::render_phuljhari(uint8_t brightness) {
#if PHULJHARI_ENABLE_STRIP
    (void)brightness;
    // 1. Gently decay previous spark embers
    fadeToBlackBy(s_strip_leds, PHULJHARI_CONFIG_STRIP_NUM_LEDS, 35);

    // 2. Spark strikes: randomly spark golden/white embers
    uint8_t num_sparks = random8(1, 4);
    for (uint8_t s = 0; s < num_sparks; ++s) {
        uint16_t idx = random16(PHULJHARI_CONFIG_STRIP_NUM_LEDS);
        if (random8() < 60) {
            // Hot white-gold spark center
            s_strip_leds[idx] = CRGB(255, 245, 210);
        } else {
            // Warm golden amber ember
            s_strip_leds[idx] = CRGB(255, random8(160, 200), random8(10, 50));
        }
    }

    // 3. Wandering subtle ambient warmth
    uint16_t wander_idx = (animation_step_ / 2) % PHULJHARI_CONFIG_STRIP_NUM_LEDS;
    s_strip_leds[wander_idx] += CRGB(40, 25, 5);
#endif
}

/* -------------------------------------------------------------------------
 * Effect 2: Anar 🎆 (Colorful Fountain-like Particles)
 * ------------------------------------------------------------------------- */
void LedController::render_anar(uint8_t brightness) {
#if PHULJHARI_ENABLE_STRIP
    (void)brightness;
    // 1. Fade previous frame to create streaming trails
    fadeToBlackBy(s_strip_leds, PHULJHARI_CONFIG_STRIP_NUM_LEDS, 45);

    // 2. Base fountain glow at LED 0
    s_strip_leds[0] = CHSV(random8(30, 60), 220, 255);
    if (PHULJHARI_CONFIG_STRIP_NUM_LEDS > 1) {
        s_strip_leds[1] = CHSV(random8(10, 45), 240, 200);
    }

    // 3. Spawn fountain particles from the base
    if (random8() < 120) {
        for (size_t i = 0; i < MAX_FOUNTAIN_PARTICLES; ++i) {
            if (!fountain_particles_[i].active) {
                fountain_particles_[i].active = true;
                fountain_particles_[i].position = 0.0f;
                // Upward velocity scaled to reach varying heights
                fountain_particles_[i].velocity = ((float)random8(100, 240)) / 100.0f;
                // Festive colors: vermilion, gold, green, magenta, cyan
                static const uint8_t FESTIVE_HUES[] = { 0, 32, 96, 140, 210, 235 };
                fountain_particles_[i].hue = FESTIVE_HUES[random8(6)];
                break;
            }
        }
    }

    // 4. Update particle physics (velocity & simulated gravity deceleration)
    for (size_t i = 0; i < MAX_FOUNTAIN_PARTICLES; ++i) {
        if (fountain_particles_[i].active) {
            fountain_particles_[i].position += fountain_particles_[i].velocity;
            fountain_particles_[i].velocity -= 0.06f; // Simulated gravity

            int pos = (int)fountain_particles_[i].position;
            if (pos >= 0 && pos < (int)PHULJHARI_CONFIG_STRIP_NUM_LEDS) {
                CRGB spark_color = CHSV(fountain_particles_[i].hue, 240, 255);
                s_strip_leds[pos] += spark_color;
            }

            if (fountain_particles_[i].position < 0.0f ||
                pos >= (int)PHULJHARI_CONFIG_STRIP_NUM_LEDS) {
                fountain_particles_[i].active = false;
            }
        }
    }
#endif
}

/* -------------------------------------------------------------------------
 * Effect 3: Patakha 💥 (Playful Pixel Burst)
 * ------------------------------------------------------------------------- */
void LedController::render_patakha(uint8_t brightness) {
#if PHULJHARI_ENABLE_STRIP
    (void)brightness;
    // 1. Rapid fade to give snappy burst pops
    fadeToBlackBy(s_strip_leds, PHULJHARI_CONFIG_STRIP_NUM_LEDS, 50);

    // 2. Trigger new burst if none is active or radius completed
    if (!burst_active_ || burst_radius_ > ((float)PHULJHARI_CONFIG_STRIP_NUM_LEDS / 2.0f)) {
        burst_active_ = true;
        burst_center_ = (float)random16(PHULJHARI_CONFIG_STRIP_NUM_LEDS / 4, (3 * PHULJHARI_CONFIG_STRIP_NUM_LEDS) / 4);
        burst_radius_ = 0.0f;
        burst_hue_ = random8(); // Random vibrant pop color
    }

    // 3. Expand shockwave radius
    burst_radius_ += 0.85f;

    int left = (int)(burst_center_ - burst_radius_);
    int right = (int)(burst_center_ + burst_radius_);

    if (left >= 0 && left < (int)PHULJHARI_CONFIG_STRIP_NUM_LEDS) {
        s_strip_leds[left] = CHSV(burst_hue_, 230, 255);
    }
    if (right >= 0 && right < (int)PHULJHARI_CONFIG_STRIP_NUM_LEDS) {
        s_strip_leds[right] = CHSV(burst_hue_ + 32, 230, 255);
    }

    // 4. Stardust sparkles around burst center
    if (random8() < 140) {
        int spark_pos = (int)burst_center_ + random8(0, 5) - 2;
        if (spark_pos >= 0 && spark_pos < (int)PHULJHARI_CONFIG_STRIP_NUM_LEDS) {
            s_strip_leds[spark_pos] = CRGB(255, 255, 255);
        }
    }
#endif
}

/* -------------------------------------------------------------------------
 * Safe Idle Renderer
 * ------------------------------------------------------------------------- */
void LedController::render_safe_idle() {
#if PHULJHARI_ENABLE_STRIP
    // Clean blackout in safe idle
    FastLED.clear(false);
#endif
}

/* -------------------------------------------------------------------------
 * Bench-Test / Onboard LED (Strictly Button-Controlled)
 * ------------------------------------------------------------------------- */
void LedController::update_onboard_led(bool button_indicator_active) {
#if PHULJHARI_ENABLE_ONBOARD_LED
    // GPIO 2 responds ONLY to valid Button A press, hold-sync, and release.
    // Strip effects, idle heartbeats, startup activity, and radio packets
    // must NEVER alter or control GPIO 2.
    bool led_on = button_indicator_active;

    uint8_t pin_level = led_on
                            ? (PHULJHARI_CONFIG_ONBOARD_ACTIVE_HIGH ? HIGH : LOW)
                            : (PHULJHARI_CONFIG_ONBOARD_ACTIVE_HIGH ? LOW : HIGH);

    digitalWrite(PHULJHARI_CONFIG_ONBOARD_LED_PIN, pin_level);
#else
    (void)button_indicator_active;
#endif
}

} // namespace receiver
} // namespace phuljhari
