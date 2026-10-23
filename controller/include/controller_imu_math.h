/**
 * @file controller_imu_math.h
 * @brief Pure mathematical helpers for IMU filtering, dead-zones, and subtle cracker tilt.
 * 
 * Hardware-independent: compiles on host Clang/GCC for unit testing and on Xtensa for firmware.
 */

#ifndef PHULJHARI_CONTROLLER_IMU_MATH_H
#define PHULJHARI_CONTROLLER_IMU_MATH_H

#include <cmath>
#include "controller_config.h"

namespace phuljhari {
namespace controller {

class ImuTiltMath {
public:
    /**
     * @brief Computes smooth, filtered, dead-zoned, and clamped visual tilt.
     * 
     * Guarantees:
     * 1. Small dead zone: values within dead zone target 0.0f (level neutral).
     * 2. Exponential Moving Average (EMA) smoothing prevents jitter.
     * 3. Smooth restoration toward neutral orientation when level.
     * 4. Clamping to [-1.0f, +1.0f] normalized.
     * 
     * @param raw_g Raw accelerometer G-force along the tilt axis.
     * @param current_filtered Current smoothed value in [-1.0f, +1.0f].
     * @param deadzone Dead zone threshold (default PHULJHARI_IMU_DEADZONE_G).
     * @param max_g Full tilt G-force threshold (default PHULJHARI_IMU_MAX_TILT_G).
     * @param alpha EMA filter factor (default PHULJHARI_IMU_EMA_ALPHA).
     * @return New smoothed tilt value in [-1.0f, +1.0f].
     */
    static inline float update_smooth_tilt(
        float raw_g,
        float current_filtered,
        float deadzone = PHULJHARI_IMU_DEADZONE_G,
        float max_g = PHULJHARI_IMU_MAX_TILT_G,
        float alpha = PHULJHARI_IMU_EMA_ALPHA
    ) {
        float abs_g = std::fabs(raw_g);
        float target = 0.0f;

        if (abs_g > deadzone) {
            float span = max_g - deadzone;
            if (span < 0.01f) span = 0.01f;
            float mag = (abs_g - deadzone) / span;
            if (mag > 1.0f) mag = 1.0f;
            target = (raw_g > 0.0f) ? mag : -mag;
        }

        // Apply EMA low-pass filter
        float smoothed = current_filtered * (1.0f - alpha) + target * alpha;

        // Clean snap to zero when resting in dead zone
        if (target == 0.0f && std::fabs(smoothed) < 0.005f) {
            smoothed = 0.0f;
        }

        // Clamp to [-1.0f, +1.0f]
        if (smoothed > 1.0f) smoothed = 1.0f;
        if (smoothed < -1.0f) smoothed = -1.0f;

        return smoothed;
    }

    /**
     * @brief Converts normalized tilt [-1.0f, +1.0f] to a clamped visual rotation angle in radians.
     * 
     * @param tilt_norm Normalized tilt value.
     * @param max_rad Maximum rotation in radians (default PHULJHARI_IMU_MAX_ROTATION_RAD).
     * @return Clamped angle in radians within [-max_rad, +max_rad].
     */
    static inline float tilt_to_rotation_rad(
        float tilt_norm,
        float max_rad = PHULJHARI_IMU_MAX_ROTATION_RAD
    ) {
        float rad = tilt_norm * max_rad;
        if (rad > max_rad) rad = max_rad;
        if (rad < -max_rad) rad = -max_rad;
        return rad;
    }
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_IMU_MATH_H */
