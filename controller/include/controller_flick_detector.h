/**
 * @file controller_flick_detector.h
 * @brief Dedicated, testable IMU flick detector for deliberate gesture animation trigger.
 * 
 * Hardware-independent: compiles natively on host Clang/GCC and on ESP32-S3.
 */

#ifndef PHULJHARI_CONTROLLER_FLICK_DETECTOR_H
#define PHULJHARI_CONTROLLER_FLICK_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <cmath>
#include "controller_config.h"

namespace phuljhari {
namespace controller {

class FlickDetector {
public:
    FlickDetector()
        : enabled_(true),
          threshold_g_(PHULJHARI_FLICK_SENS_MED_G),
          cooldown_ms_(PHULJHARI_FLICK_COOLDOWN_MS),
          last_flick_time_ms_(0),
          prev_magnitude_(1.0f),
          prev_sample_time_ms_(0) {}

    void init() {
        last_flick_time_ms_ = 0;
        prev_magnitude_ = 1.0f;
        prev_sample_time_ms_ = 0;
    }

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    void set_threshold(float threshold_g) { threshold_g_ = threshold_g; }
    float get_threshold() const { return threshold_g_; }

    void set_cooldown(uint32_t cooldown_ms) { cooldown_ms_ = cooldown_ms; }
    uint32_t get_cooldown() const { return cooldown_ms_; }

    uint32_t get_last_flick_time() const { return last_flick_time_ms_; }

    /**
     * @brief Processes an accelerometer sample and detects deliberate flick gestures.
     * 
     * Requirements:
     * 1. Must be enabled.
     * 2. Cooldown period must have elapsed since previous flick.
     * 3. Total instantaneous G-force magnitude must exceed threshold_g_.
     * 4. Must exhibit rapid dynamic onset (jerk), distinguishing it from static tilt.
     * 
     * @param ax Acceleration along X in G.
     * @param ay Acceleration along Y in G.
     * @param az Acceleration along Z in G.
     * @param current_time_ms Current timestamp in milliseconds.
     * @return true if a deliberate flick gesture occurred, false otherwise.
     */
    bool process_sample(float ax, float ay, float az, uint32_t current_time_ms) {
        float mag = std::sqrt(ax * ax + ay * ay + az * az);

        // If time gap since last sample is large (>200ms), baseline resets to earth gravity (1.0G)
        if (prev_sample_time_ms_ > 0 && (current_time_ms - prev_sample_time_ms_ > 200)) {
            prev_magnitude_ = 1.0f;
        }
        prev_sample_time_ms_ = current_time_ms;

        float delta = std::fabs(mag - prev_magnitude_);
        prev_magnitude_ = mag;

        if (!enabled_) {
            return false;
        }

        // Check cooldown
        if (last_flick_time_ms_ > 0 && (current_time_ms - last_flick_time_ms_ < cooldown_ms_)) {
            return false;
        }

        // Deliberate flick criteria: magnitude exceeds threshold AND dynamic onset exists
        // (Static tilt has mag ~ 1.0G and delta ~ 0, so it will never trigger)
        if (mag >= threshold_g_ && delta >= 0.25f) {
            last_flick_time_ms_ = current_time_ms;
            return true;
        }

        return false;
    }

private:
    bool enabled_;
    float threshold_g_;
    uint32_t cooldown_ms_;
    uint32_t last_flick_time_ms_;
    float prev_magnitude_;
    uint32_t prev_sample_time_ms_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_FLICK_DETECTOR_H */
