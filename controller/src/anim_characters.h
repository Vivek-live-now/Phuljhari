/**
 * @file anim_characters.h
 * @brief Character sprite engines, particle systems, and festive backgrounds for Phuljhari 2.0 ✨
 * 
 * Target Display: 135 x 240 Portrait M5Canvas
 */

#ifndef PHULJHARI_ANIM_CHARACTERS_H
#define PHULJHARI_ANIM_CHARACTERS_H

#include <stdint.h>
#include <stddef.h>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

#include "phuljhari_protocol.h"
#include "controller_config.h"
#include "character_types.h"
#include "controller_settings.h"

namespace phuljhari {
namespace controller {

class AnimCharacterEngine {
public:
    AnimCharacterEngine();

    void init();

#if defined(ARDUINO)
    /**
     * @brief Renders the startup splash logo & animation in portrait mode.
     */
    void render_startup_screen(M5Canvas& canvas, uint32_t step);

    /**
     * @brief Renders the background theme (Rooftop, Courtyard, Bazaar).
     */
    void render_background(M5Canvas& canvas, BackgroundTheme theme, bool power_save, uint32_t current_time_ms);

    /**
     * @brief Renders glowing rangoli floor mandala layer.
     */
    void render_rangoli(M5Canvas& canvas, bool active, uint32_t current_time_ms);

    /**
     * @brief Renders the selected character in either IDLE, PLAYING, or COOLDOWN state.
     */
    void render_character(
        M5Canvas& canvas,
        CharacterId character,
        AnimationState state,
        float tilt_norm,
        float tilt_angle_rad,
        uint32_t elapsed_ms,
        uint32_t current_time_ms
    );

    /**
     * @brief Deposits a glowing rangoli petal upon celebration completion.
     */
    void deposit_rangoli_petal(uint16_t color);

private:
    /* Background Renderers */
    void render_bg_rooftop(M5Canvas& canvas, bool power_save, uint32_t current_time_ms);
    void render_bg_courtyard(M5Canvas& canvas, bool power_save, uint32_t current_time_ms);
    void render_bg_bazaar(M5Canvas& canvas, bool power_save, uint32_t current_time_ms);

    /* Individual Character Renderers */
    void render_phuljhari(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
    void render_anar(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
    void render_sky_rocket(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
    void render_garland(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
    void render_bomb(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
    void render_chakri(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
    void render_candle(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
    void render_diya(M5Canvas& canvas, int cx, int cy, AnimationState state, float theta, uint32_t elapsed, uint32_t now);
#endif

private:
    /* Sparkle Particle System */
    struct SparkleParticle {
        float x, y;
        float vx, vy;
        uint16_t color;
        uint8_t life;
        bool active;
    };
    static const size_t MAX_SPARKLES = 28;
    SparkleParticle sparkles_[MAX_SPARKLES];

    /* Fountain Particle System */
    struct FountainParticle {
        float x, y;
        float vx, vy;
        uint16_t color;
        bool active;
    };
    static const size_t MAX_FOUNTAIN = 36;
    FountainParticle fountain_[MAX_FOUNTAIN];

    /* Rangoli Floor Petals */
    struct RangoliPetal {
        float angle;
        float radius;
        uint16_t color;
        uint8_t alpha; // 0-255 fade
        bool active;
    };
    static const size_t MAX_RANGOLI = 8;
    RangoliPetal rangoli_[MAX_RANGOLI];
    size_t rangoli_next_idx_;

    uint32_t frame_count_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_ANIM_CHARACTERS_H */
