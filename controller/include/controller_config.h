/**
 * @file controller_config.h
 * @brief Configuration and hardware mappings for M5StickS3 Controller ✨
 * 
 * Hardware Specifications:
 * - SoC: ESP32-S3-PICO-1-N8R8 (8MB Flash, 8MB OPI PSRAM)
 * - Display: 1.14" ST7789P3 TFT LCD (135 x 240, operated in landscape 240 x 135)
 * - Buttons: BtnA (Front thumb button), BtnB (Side navigation button)
 * - IMU: 6-Axis Motion Sensor (BMI270) via M5Unified (I2C)
 * - Power: Built-in PMIC (AXP2101 / SY8089) with battery level sensing
 */

#ifndef PHULJHARI_CONTROLLER_CONFIG_H
#define PHULJHARI_CONTROLLER_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Display & Rendering Configuration (Portrait 135 x 240)
 * ------------------------------------------------------------------------- */
/** Display width in portrait orientation. */
#define PHULJHARI_DISP_WIDTH             135
/** Display height in portrait orientation. */
#define PHULJHARI_DISP_HEIGHT            240
/** Display rotation: 0 = Portrait with Front Button A at bottom. */
#define PHULJHARI_DISP_ROTATION          0

/** Normal UI Target Frame Rate (35 FPS -> ~28 ms per frame). */
#define PHULJHARI_UI_FRAME_INTERVAL_MS   28
/** Power-Saving UI Frame Rate (18 FPS -> ~55 ms per frame). */
#define PHULJHARI_UI_POWER_SAVE_INTERVAL_MS 55

/* -------------------------------------------------------------------------
 * Wireless & Radio Settings (ESP-NOW)
 * ------------------------------------------------------------------------- */
/** Wi-Fi Channel (must match Receiver). */
#define PHULJHARI_RADIO_CHANNEL          1

/** Heartbeat ping interval when idle (milliseconds). */
#define PHULJHARI_PING_INTERVAL_MS       2000

/** Time after last RX before transitioning to LINK_RECONNECTING (milliseconds). */
#define PHULJHARI_RECONNECT_TIMEOUT_MS   3500

/** Time after last RX before transitioning to LINK_OFFLINE (milliseconds). */
#define PHULJHARI_OFFLINE_TIMEOUT_MS     7000

/** Maximum transmission retry attempts for effect commands. */
#define PHULJHARI_MAX_TX_RETRIES         3

/** Interval between retry attempts (milliseconds). */
#define PHULJHARI_RETRY_INTERVAL_MS      200

/** Duration to display celebratory ACK confirmation toast (milliseconds). */
#define PHULJHARI_ACK_TOAST_DURATION_MS  1600

/* -------------------------------------------------------------------------
 * Live Button Synchronization Timings
 * ------------------------------------------------------------------------- */
/** Periodic button hold sync refresh interval (milliseconds). */
#define PHULJHARI_BUTTON_SYNC_REFRESH_MS 180

/* -------------------------------------------------------------------------
 * IMU & Visual Tilt Settings (BMI270)
 * ------------------------------------------------------------------------- */
/** Accelerometer low-pass filter factor (Exponential Moving Average). */
#define PHULJHARI_IMU_EMA_ALPHA          0.20f

/** Tilt dead zone (G-force threshold below which tilt is treated as level neutral). */
#define PHULJHARI_IMU_DEADZONE_G         0.12f

/** G-force magnitude corresponding to maximum visual tilt excursion. */
#define PHULJHARI_IMU_MAX_TILT_G         0.65f

/** Maximum visual rotation angle in radians (~20 degrees = 0.35 rad). */
#define PHULJHARI_IMU_MAX_ROTATION_RAD   0.35f

/** Flick sensitivity presets (G-force threshold). */
#define PHULJHARI_FLICK_SENS_LOW_G       1.65f
#define PHULJHARI_FLICK_SENS_MED_G       1.35f
#define PHULJHARI_FLICK_SENS_HIGH_G      1.10f

/** Minimum cooldown between deliberate animation start flicks (milliseconds). */
#define PHULJHARI_FLICK_COOLDOWN_MS      700

/** Cooldown duration after animation finishes before allowing restart (milliseconds). */
#define PHULJHARI_ANIM_COOLDOWN_MS       500

/** Long press duration for Button B to enter Settings (milliseconds). */
#define PHULJHARI_BTN_B_SETTINGS_HOLD_MS 600

#endif /* PHULJHARI_CONTROLLER_CONFIG_H */
