/**
 * @file character_types.h
 * @brief Character identifiers, metadata descriptors, and animation states for Phuljhari 2.0 ✨
 */

#ifndef PHULJHARI_CHARACTER_TYPES_H
#define PHULJHARI_CHARACTER_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include "phuljhari_protocol.h"

namespace phuljhari {
namespace controller {

/**
 * @brief Selectable Festive Characters in Pocket Diwali Festival
 */
enum CharacterId : uint8_t {
    CHAR_PHULJHARI   = 0, /**< Golden sparkles & gentle tilt-responsive wand */
    CHAR_ANAR        = 1, /**< Colorful fountain pot with rising particles */
    CHAR_SKY_ROCKET  = 2, /**< Ascending rocket with trail & cartoon starburst */
    CHAR_GARLAND     = 3, /**< Festive ladi with cascading staccato pops */
    CHAR_BOMB        = 4, /**< Friendly cartoon sutli bomb mascot with confetti burst */
    CHAR_CHAKRI      = 5, /**< Whirling ground wheel with spiral light trails */
    CHAR_CANDLE      = 6, /**< Festival candle with gently flickering flame */
    CHAR_DIYA        = 7, /**< Traditional clay lamp with warm golden glow */
    CHAR_COUNT       = 8
};

/**
 * @brief Character Animation Lifecycle States
 */
enum AnimationState : uint8_t {
    CHAR_STATE_IDLE     = 0, /**< Calm idle pose with subtle breathing & tip glow */
    CHAR_STATE_PLAYING  = 1, /**< Active celebration animation & particle simulation */
    CHAR_STATE_COOLDOWN = 2  /**< Particles fade away smoothly before returning to idle */
};

/**
 * @brief Sound Effect Identifiers
 */
enum SoundEffectId : uint8_t {
    SFX_NONE         = 0,
    SFX_UI_CLICK     = 1,
    SFX_PHULJHARI    = 2,
    SFX_ANAR         = 3,
    SFX_SKY_ROCKET   = 4,
    SFX_GARLAND      = 5,
    SFX_BOMB         = 6,
    SFX_CHAKRI       = 7,
    SFX_CANDLE_DIYA  = 8
};

/**
 * @brief Immutable Metadata Descriptor for Each Character
 */
struct CharacterDescriptor {
    CharacterId id;
    const char* display_name;
    const char* subtitle;
    uint16_t duration_ms;              /**< Duration of active PLAYING state */
    protocol::EffectId wireless_effect;/**< Mapped wireless protocol effect ID */
    SoundEffectId sound_id;
};

static const CharacterDescriptor CHARACTER_REGISTRY[CHAR_COUNT] = {
    { CHAR_PHULJHARI,  "PHULJHARI",   "Golden Wand",    3500, protocol::EFFECT_ID_PHULJHARI, SFX_PHULJHARI },
    { CHAR_ANAR,       "ANAR",        "Fountain Pot",   4200, protocol::EFFECT_ID_ANAR,      SFX_ANAR },
    { CHAR_SKY_ROCKET, "SKY ROCKET",  "Star Rocket",    3000, protocol::EFFECT_ID_PATAKHA,   SFX_SKY_ROCKET },
    { CHAR_GARLAND,    "GARLAND",     "Ladi Pops",      3800, protocol::EFFECT_ID_PATAKHA,   SFX_GARLAND },
    { CHAR_BOMB,       "PATAKHA BOMB","Friendly Mascot",2600, protocol::EFFECT_ID_PATAKHA,   SFX_BOMB },
    { CHAR_CHAKRI,     "CHAKRI",      "Spinning Wheel", 4000, protocol::EFFECT_ID_ANAR,      SFX_CHAKRI },
    { CHAR_CANDLE,     "CANDLE",      "Wax Flame",      3500, protocol::EFFECT_ID_PHULJHARI, SFX_CANDLE_DIYA },
    { CHAR_DIYA,       "DIYA",        "Sacred Lamp",    3500, protocol::EFFECT_ID_PHULJHARI, SFX_CANDLE_DIYA }
};

static inline const CharacterDescriptor& get_character_descriptor(CharacterId id) {
    if (id >= CHAR_COUNT) {
        return CHARACTER_REGISTRY[0];
    }
    return CHARACTER_REGISTRY[id];
}

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CHARACTER_TYPES_H */
