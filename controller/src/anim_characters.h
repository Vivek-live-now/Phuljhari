/**
 * @file anim_characters.h
 * @brief Character sprite engines and particle animations for M5StickS3 Controller ✨
 * 
 * Characters:
 * - Phuljhari: Golden sparkles & gentle twinkling sparkler wand mascot
 * - Anar: Upward colorful particle fountain mascot with physics
 * - Patakha: Cute bouncy cartoon mascot with celebratory pixel-bursts
 */

#ifndef PHULJHARI_ANIM_CHARACTERS_H
#define PHULJHARI_ANIM_CHARACTERS_H

#include <stdint.h>
#include <stddef.h>
#include <M5Unified.h>

#include "phuljhari_protocol.h"
#include "controller_config.h"

namespace phuljhari {
namespace controller {

class AnimCharacterEngine {
public:
    AnimCharacterEngine();

    void init();

    /**
     * @brief Renders the startup splash logo & animation onto canvas.
     */
    void render_startup_screen(M5Canvas& canvas, uint32_t step);

    /**
     * @brief Renders the active animated character and particle simulation.
     */
    void render_character(
        M5Canvas& canvas,
        protocol::EffectId effect,
        float tilt_norm,
        float tilt_angle_rad,
        bool trigger_virtual_burst,
        uint32_t current_time_ms
    );

private:
    void render_phuljhari(M5Canvas& canvas, int cx, int cy, float tilt_norm, float tilt_angle_rad, uint32_t current_time_ms);
    void render_anar(M5Canvas& canvas, int cx, int cy, float tilt_norm, float tilt_angle_rad, uint32_t current_time_ms);
    void render_patakha(M5Canvas& canvas, int cx, int cy, float tilt_norm, float tilt_angle_rad, uint32_t current_time_ms);

    /* Particles for Phuljhari sparkles */
    struct SparkleParticle {
        float x, y;
        float vx, vy;
        uint16_t color;
        uint8_t life;
        bool active;
    };
    static const size_t MAX_SPARKLES = 16;
    SparkleParticle sparkles_[MAX_SPARKLES];

    /* Particles for Anar fountain */
    struct FountainParticle {
        float x, y;
        float vx, vy;
        uint16_t color;
        bool active;
    };
    static const size_t MAX_FOUNTAIN = 24;
    FountainParticle fountain_[MAX_FOUNTAIN];

    /* Shockwave particles for Patakha bursts */
    struct BurstWave {
        float cx, cy;
        float radius;
        uint16_t color;
        bool active;
    };
    static const size_t MAX_BURSTS = 3;
    BurstWave bursts_[MAX_BURSTS];

    uint32_t frame_count_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_ANIM_CHARACTERS_H */
