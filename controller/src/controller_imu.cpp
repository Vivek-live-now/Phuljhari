/**
 * @file controller_imu.cpp
 * @brief Implementation of BMI270 IMU tracking and flick-to-start detection for Phuljhari 2.0 ✨
 */

#include "controller_imu.h"
#include <cmath>

#if defined(ARDUINO)
#include <Arduino.h>
#include <M5Unified.h>
#endif

namespace phuljhari {
namespace controller {

ControllerImu::ControllerImu()
    : imu_available_(false),
      filtered_ax_(0.0f),
      filtered_ay_(0.0f),
      filtered_az_(0.0f),
      filtered_tilt_norm_(0.0f),
      tilt_angle_rad_(0.0f) {}

bool ControllerImu::begin() {
#if defined(ARDUINO)
    Serial.println("[IMU] Initializing BMI270 6-Axis Motion Sensor for Portrait Phuljhari 2.0...");

    if (!M5.Imu.isEnabled()) {
        Serial.println("[IMU] WARNING: M5.Imu reported disabled or absent on hardware.");
        imu_available_ = false;
        return false;
    }

    imu_available_ = true;
    flick_detector_.init();
    Serial.println("[IMU] BMI270 IMU ready (Portrait tilt & Flick-to-start active).");
    return true;
#else
    imu_available_ = true;
    flick_detector_.init();
    return true;
#endif
}

void ControllerImu::update(ControllerStateMachine& csm, uint32_t current_time_ms) {
    if (!imu_available_) return;

    float raw_ax = 0.0f, raw_ay = 0.0f, raw_az = 0.0f;

#if defined(ARDUINO)
    float raw_gx = 0.0f, raw_gy = 0.0f, raw_gz = 0.0f;
    M5.Imu.update();
    M5.Imu.getAccel(&raw_ax, &raw_ay, &raw_az);
    M5.Imu.getGyro(&raw_gx, &raw_gy, &raw_gz);
#endif

    // 1. Exponential Moving Average (EMA) low-pass filter to smooth tilt jitter
    filtered_ax_ = filtered_ax_ * (1.0f - PHULJHARI_IMU_EMA_ALPHA) + raw_ax * PHULJHARI_IMU_EMA_ALPHA;
    filtered_ay_ = filtered_ay_ * (1.0f - PHULJHARI_IMU_EMA_ALPHA) + raw_ay * PHULJHARI_IMU_EMA_ALPHA;
    filtered_az_ = filtered_az_ * (1.0f - PHULJHARI_IMU_EMA_ALPHA) + raw_az * PHULJHARI_IMU_EMA_ALPHA;

    // 2. Compute smooth visual tilt along display portrait axis
    // In portrait rotation 0, left/right tilt corresponds to -filtered_ay_ or filtered_ax_
    filtered_tilt_norm_ = ImuTiltMath::update_smooth_tilt(filtered_ax_, filtered_tilt_norm_);
    tilt_angle_rad_ = ImuTiltMath::tilt_to_rotation_rad(filtered_tilt_norm_);

    // 3. Update visual tilt state in CSM (never changes selection or transmits)
    csm.set_sim_tilt(filtered_tilt_norm_, tilt_angle_rad_);

    // 4. Deliberate Flick-to-Start Detection
    // Synchronize settings from CSM (enabled flag & dynamic threshold)
    flick_detector_.set_enabled(csm.get_settings().flick_enabled);
    flick_detector_.set_threshold(csm.get_settings().get_flick_threshold_g());

    if (flick_detector_.process_sample(raw_ax, raw_ay, raw_az, current_time_ms)) {
        // Deliberate flick gesture detected!
        // Start animation if in IDLE (does not change selection, handles sound & wireless safely)
        csm.trigger_animation(current_time_ms);
    }
}

} // namespace controller
} // namespace phuljhari
