/**
 * @file led_controller.h
 * @brief FastLED driver and decorative animation engine for Project Phuljhari Receiver ✨
 * 
 * Hardware:
 * - WS2812B / SK6812 Addressable RGB LED Strip
 * - Configurable Onboard Indicator LED (Bench-testing)
 * 
 * Safety:
 * Strictly harmless decorative lighting effects.
 * Max brightness capped to prevent USB power brownouts.
 * Zero pyrotechnic, arc, or high-energy actuation logic.
 */

#ifndef PHULJHARI_LED_CONTROLLER_H
#define PHULJHARI_LED_CONTROLLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "receiver_config.h"

namespace phuljhari {
namespace receiver {

class LedController {
public:
    LedController();

    /**
     * @brief Configures GPIO pins, initializes FastLED, and sets all outputs to inactive.
     */
    void begin();

    /**
     * @brief Updates animations and renders LED frame.
     * Non-blocking, rate-limited to ~50 FPS.
     * @param state Current ReceiverState.
     * @param effect Active EffectId.
     * @param intensity_pct Effect intensity (1-100%).
     * @param current_time_ms Current time in milliseconds.
     */
    void update(
        protocol::ReceiverState state,
        protocol::EffectId effect,
        uint8_t intensity_pct,
        bool button_indicator_active,
        uint32_t current_time_ms
    );

    /**
     * @brief Immediately blanks all LEDs and turns off onboard indicator.
     */
    void set_all_inactive();

private:
    /* Decorative Effect Renderers */
    void render_phuljhari(uint8_t brightness);
    void render_anar(uint8_t brightness);
    void render_patakha(uint8_t brightness);
    void render_safe_idle();

    /* Bench-test Onboard LED Renderer (Strictly Button-Controlled) */
    void update_onboard_led(bool button_indicator_active);

    uint32_t last_frame_ms_;
    uint32_t animation_step_;

    /* Particle state for Anar fountain simulation */
    struct FountainParticle {
        float position;
        float velocity;
        uint8_t hue;
        bool active;
    };
    static const size_t MAX_FOUNTAIN_PARTICLES = 8;
    FountainParticle fountain_particles_[MAX_FOUNTAIN_PARTICLES];

    /* Burst state for Patakha */
    uint32_t last_burst_ms_;
    float burst_center_;
    float burst_radius_;
    uint8_t burst_hue_;
    bool burst_active_;
};

} // namespace receiver
} // namespace phuljhari

#endif /* PHULJHARI_LED_CONTROLLER_H */
