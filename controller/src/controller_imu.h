/**
 * @file controller_imu.h
 * @brief 6-Axis IMU (BMI270) driver, EMA filtering, and flick detection for Phuljhari 2.0 ✨
 */

#ifndef PHULJHARI_CONTROLLER_IMU_H
#define PHULJHARI_CONTROLLER_IMU_H

#include <stdint.h>
#include <stdbool.h>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

#include "controller_config.h"
#include "controller_imu_math.h"
#include "controller_flick_detector.h"
#include "controller_state_machine.h"

namespace phuljhari {
namespace controller {

class ControllerImu {
public:
    ControllerImu();

    /**
     * @brief Initializes the BMI270 6-axis IMU via M5Unified.
     * @return true if IMU hardware was detected and initialized.
     */
    bool begin();

    /**
     * @brief Reads accelerometer, applies EMA filtering, dead-zones, and clamped rotation,
     *        and detects deliberate flick-to-start gestures.
     *        Does NOT select crackers.
     * @param csm Reference to controller state machine.
     * @param current_time_ms Current timestamp.
     */
    void update(ControllerStateMachine& csm, uint32_t current_time_ms);

    float get_accel_x() const { return filtered_ax_; }
    float get_accel_y() const { return filtered_ay_; }
    float get_accel_z() const { return filtered_az_; }
    float get_tilt_norm() const { return filtered_tilt_norm_; }
    float get_tilt_angle_rad() const { return tilt_angle_rad_; }
    bool is_available() const { return imu_available_; }

    FlickDetector& get_flick_detector() { return flick_detector_; }

private:
    bool imu_available_;

    /* Filtered accelerometer states (EMA) */
    float filtered_ax_;
    float filtered_ay_;
    float filtered_az_;

    /* Smoothed visual tilt states */
    float filtered_tilt_norm_;
    float tilt_angle_rad_;

    /* Dedicated flick detector */
    FlickDetector flick_detector_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_IMU_H */
