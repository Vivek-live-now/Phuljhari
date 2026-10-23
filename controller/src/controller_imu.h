/**
 * @file controller_imu.h
 * @brief 6-Axis IMU (BMI270) driver, EMA filtering, and gesture engine for M5StickS3 Controller ✨
 */

#ifndef PHULJHARI_CONTROLLER_IMU_H
#define PHULJHARI_CONTROLLER_IMU_H

#include <stdint.h>
#include <stdbool.h>
#include <M5Unified.h>

#include "controller_config.h"
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
     * @brief Reads accelerometer and gyroscope, applies EMA filtering and dead-zones,
     *        and detects deliberate wrist gestures.
     * @param csm Reference to controller state machine.
     * @param current_time_ms Current timestamp.
     */
    void update(ControllerStateMachine& csm, uint32_t current_time_ms);

    float get_accel_x() const { return filtered_ax_; }
    float get_accel_y() const { return filtered_ay_; }
    float get_accel_z() const { return filtered_az_; }
    bool is_available() const { return imu_available_; }

private:
    bool imu_available_;

    /* Filtered accelerometer states (EMA) */
    float filtered_ax_;
    float filtered_ay_;
    float filtered_az_;

    /* Gyroscope states (deg/s) */
    float gyro_gx_;
    float gyro_gy_;
    float gyro_gz_;

    /* Gesture debounce & cooldown timers */
    uint32_t last_tilt_gesture_ms_;
    uint32_t last_flick_gesture_ms_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_IMU_H */
