/**
 * @file controller_imu.cpp
 * @brief Implementation of BMI270 IMU motion tracking and gesture controls.
 */

#include "controller_imu.h"
#include <cmath>

namespace phuljhari {
namespace controller {

ControllerImu::ControllerImu()
    : imu_available_(false),
      filtered_ax_(0.0f),
      filtered_ay_(0.0f),
      filtered_az_(0.0f),
      filtered_tilt_norm_(0.0f),
      tilt_angle_rad_(0.0f),
      last_flick_telemetry_ms_(0) {}

bool ControllerImu::begin() {
    Serial.println("[IMU] Initializing BMI270 6-Axis Motion Sensor...");

    if (!M5.Imu.isEnabled()) {
        Serial.println("[IMU] WARNING: M5.Imu report disabled or absent on hardware.");
        imu_available_ = false;
        return false;
    }

    imu_available_ = true;
    Serial.println("[IMU] BMI270 IMU ready for subtle cracker visual tilt.");
    return true;
}

void ControllerImu::update(ControllerStateMachine& csm, uint32_t current_time_ms) {
    if (!imu_available_) return;

    // 1. Read raw hardware sensor data
    float raw_ax = 0.0f, raw_ay = 0.0f, raw_az = 0.0f;
    float raw_gx = 0.0f, raw_gy = 0.0f, raw_gz = 0.0f;

    M5.Imu.update();
    M5.Imu.getAccel(&raw_ax, &raw_ay, &raw_az);
    M5.Imu.getGyro(&raw_gx, &raw_gy, &raw_gz);

    // 2. Exponential Moving Average (EMA) low-pass filter to smooth jitter
    filtered_ax_ = filtered_ax_ * (1.0f - PHULJHARI_IMU_EMA_ALPHA) + raw_ax * PHULJHARI_IMU_EMA_ALPHA;
    filtered_ay_ = filtered_ay_ * (1.0f - PHULJHARI_IMU_EMA_ALPHA) + raw_ay * PHULJHARI_IMU_EMA_ALPHA;
    filtered_az_ = filtered_az_ * (1.0f - PHULJHARI_IMU_EMA_ALPHA) + raw_az * PHULJHARI_IMU_EMA_ALPHA;

    // 3. Compute smooth visual tilt along display landscape orientation
    // When holding M5StickS3 in landscape rotation 1, left/right tilt corresponds to filtered_ax_
    filtered_tilt_norm_ = ImuTiltMath::update_smooth_tilt(filtered_ax_, filtered_tilt_norm_);
    tilt_angle_rad_ = ImuTiltMath::tilt_to_rotation_rad(filtered_tilt_norm_);

    // 4. Update visual tilt state in ControllerStateMachine
    // Strictly controls visual rotation and sparkle direction.
    // DOES NOT change selected cracker or send effect commands!
    csm.set_sim_tilt(filtered_tilt_norm_, tilt_angle_rad_);

    // 5. Deliberate flick/shake triggers on-screen simulator telemetry only (never changes selection or transmits)
    float total_g = sqrt(raw_ax * raw_ax + raw_ay * raw_ay + raw_az * raw_az);
    if (total_g > PHULJHARI_IMU_FLICK_THRESHOLD_G) {
        if (current_time_ms - last_flick_telemetry_ms_ >= PHULJHARI_IMU_GESTURE_COOLDOWN_MS) {
            csm.trigger_sim_virtual_burst();
            last_flick_telemetry_ms_ = current_time_ms;
        }
    }
}

} // namespace controller
} // namespace phuljhari
