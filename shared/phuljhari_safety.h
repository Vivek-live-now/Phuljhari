/**
 * @file phuljhari_safety.h
 * @brief Safety boundaries and policies for Project Phuljhari ✨
 * 
 * ============================================================================
 * SAFETY & HARDWARE BOUNDARY POLICY
 * ============================================================================
 * Project Phuljhari is designed EXCLUSIVELY for harmless decorative lighting
 * (addressable RGB LEDs, low-voltage indicator LEDs, or decorative LED modules).
 *
 * STRICT BOUNDARIES:
 * - DO NOT connect or control any pyrotechnic, ignition, electric match, e-match,
 *   heating coil, nichrome wire, or arc discharge device.
 * - DO NOT implement ignition-control interfaces or pyrotechnic actuation abstractions.
 * - DO NOT repurpose relay outputs or low-voltage drivers for high-energy discharge.
 * - Initialize all output pins and PWM channels to INACTIVE (LOW / Off) state
 *   during boot, error conditions, communication loss, and watchdog timeouts.
 * - Hardware loads are strictly limited to low-voltage demonstration loads (<= 5V DC).
 * - Keep LED power within ratings; cap maximum brightness to prevent USB brownouts.
 * ============================================================================
 */

#ifndef PHULJHARI_SAFETY_H
#define PHULJHARI_SAFETY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
namespace phuljhari {
namespace safety {
#endif

/* -------------------------------------------------------------------------
 * Decorative Effect Timing Boundaries (Milliseconds)
 * ------------------------------------------------------------------------- */
/** Absolute minimum duration for a decorative effect (100 ms). */
#define PHULJHARI_SAFETY_MIN_DURATION_MS        ((uint16_t)100)

/** Absolute maximum duration for a single decorative effect activation (15000 ms = 15 s). */
#define PHULJHARI_SAFETY_MAX_DURATION_MS        ((uint16_t)15000)

/** Default safe duration when none or invalid is specified (5000 ms = 5 s). */
#define PHULJHARI_SAFETY_DEFAULT_DURATION_MS    ((uint16_t)5000)

/** Watchdog failsafe timeout: if no valid frame/heartbeat received within this window,
 *  all decorative outputs automatically revert to safe inactive idle (20000 ms = 20 s). */
#define PHULJHARI_SAFETY_WATCHDOG_TIMEOUT_MS    ((uint32_t)20000)

/* -------------------------------------------------------------------------
 * Decorative LED Brightness / Intensity Boundaries
 * ------------------------------------------------------------------------- */
/** Minimum valid intensity percentage. */
#define PHULJHARI_SAFETY_MIN_INTENSITY_PCT      ((uint8_t)1)

/** Maximum allowed intensity percentage (100%). */
#define PHULJHARI_SAFETY_MAX_INTENSITY_PCT      ((uint8_t)100)

/** Default safe intensity percentage (80%). */
#define PHULJHARI_SAFETY_DEFAULT_INTENSITY_PCT  ((uint8_t)80)

/** Maximum allowed 8-bit FastLED master brightness value (0-255).
 *  Capped at 180 to prevent excessive current draw and USB power brownouts. */
#define PHULJHARI_SAFETY_MAX_LED_BRIGHTNESS     ((uint8_t)180)

/* -------------------------------------------------------------------------
 * Safe Output State Definitions
 * ------------------------------------------------------------------------- */
/** Logical output state for inactive/safe condition (LOW). */
#define PHULJHARI_OUTPUT_STATE_INACTIVE         ((uint8_t)0)

/** Logical output state for active condition (HIGH). */
#define PHULJHARI_OUTPUT_STATE_ACTIVE           ((uint8_t)1)

/* -------------------------------------------------------------------------
 * Safety Helper Functions (C & C++ compatible)
 * ------------------------------------------------------------------------- */

/**
 * @brief Checks if an effect duration is within safe boundaries.
 * @param duration_ms Requested duration in milliseconds.
 * @return true if safe, false if outside bounds.
 */
static inline bool is_duration_safe(uint16_t duration_ms) {
    return (duration_ms >= PHULJHARI_SAFETY_MIN_DURATION_MS &&
            duration_ms <= PHULJHARI_SAFETY_MAX_DURATION_MS);
}

/**
 * @brief Clamps an effect duration strictly within safe boundaries.
 * @param duration_ms Requested duration in milliseconds.
 * @return Duration clamped between MIN and MAX.
 */
static inline uint16_t clamp_duration(uint16_t duration_ms) {
    if (duration_ms < PHULJHARI_SAFETY_MIN_DURATION_MS) {
        return PHULJHARI_SAFETY_MIN_DURATION_MS;
    }
    if (duration_ms > PHULJHARI_SAFETY_MAX_DURATION_MS) {
        return PHULJHARI_SAFETY_MAX_DURATION_MS;
    }
    return duration_ms;
}

/**
 * @brief Checks if an effect intensity percentage is within safe boundaries.
 * @param intensity_pct Requested intensity percentage (1-100).
 * @return true if safe, false if outside bounds.
 */
static inline bool is_intensity_safe(uint8_t intensity_pct) {
    return (intensity_pct >= PHULJHARI_SAFETY_MIN_INTENSITY_PCT &&
            intensity_pct <= PHULJHARI_SAFETY_MAX_INTENSITY_PCT);
}

/**
 * @brief Clamps an effect intensity percentage strictly within safe boundaries.
 * @param intensity_pct Requested intensity percentage.
 * @return Intensity clamped between 1% and 100%.
 */
static inline uint8_t clamp_intensity(uint8_t intensity_pct) {
    if (intensity_pct < PHULJHARI_SAFETY_MIN_INTENSITY_PCT) {
        return PHULJHARI_SAFETY_MIN_INTENSITY_PCT;
    }
    if (intensity_pct > PHULJHARI_SAFETY_MAX_INTENSITY_PCT) {
        return PHULJHARI_SAFETY_MAX_INTENSITY_PCT;
    }
    return intensity_pct;
}

/**
 * @brief Converts safe intensity percentage (1-100) to capped FastLED brightness (0-180).
 * @param intensity_pct Percentage (1-100).
 * @return 8-bit brightness scaled safely to not exceed PHULJHARI_SAFETY_MAX_LED_BRIGHTNESS.
 */
static inline uint8_t intensity_to_safe_brightness(uint8_t intensity_pct) {
    uint8_t clamped = clamp_intensity(intensity_pct);
    uint32_t scaled = ((uint32_t)clamped * (uint32_t)PHULJHARI_SAFETY_MAX_LED_BRIGHTNESS) / 100U;
    return (uint8_t)scaled;
}

#ifdef __cplusplus
} // namespace safety
} // namespace phuljhari
#endif

#endif /* PHULJHARI_SAFETY_H */
