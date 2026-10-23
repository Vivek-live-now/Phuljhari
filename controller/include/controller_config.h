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

/** Heartbeat ping interval when idle & connected (milliseconds). */
#define PHULJHARI_PING_INTERVAL_MS       2000

/** Heartbeat ping interval when receiver is offline (milliseconds, saves battery). */
#define PHULJHARI_OFFLINE_PING_INTERVAL_MS 5000

/* -------------------------------------------------------------------------
 * Power Management, Frequency Scaling & Inactivity Timings
 * ------------------------------------------------------------------------- */
/** Target CPU frequency in normal mode (160 MHz saves ~35% dynamic power vs 240 MHz). */
#define PHULJHARI_CPU_FREQ_NORMAL_MHZ    160

/** Target CPU frequency in power-save mode (80 MHz saves ~55% dynamic power vs 240 MHz). */
#define PHULJHARI_CPU_FREQ_POWER_SAVE_MHZ 80

/** Wi-Fi TX Power limit in dBm (11 dBm saves ~50% peak TX surge vs 20 dBm). */
#define PHULJHARI_WIFI_TX_POWER_DBM      11

/** Inactivity duration before dimming screen to save power (milliseconds). */
#define PHULJHARI_INACTIVITY_DIM_MS      30000

/** Backlight level when screen is auto-dimmed (low power). */
#define PHULJHARI_BRIGHTNESS_DIM         15

/** Inactivity duration before blanking screen backlight (milliseconds). */
#define PHULJHARI_INACTIVITY_BLANK_MS    90000

/** Inactivity duration before automatic hardware power-off via PMIC (milliseconds). */
#define PHULJHARI_AUTO_POWER_OFF_MS      300000

/** Loop sleep time in milliseconds during active rendering (allows FreeRTOS idle sleep). */
#define PHULJHARI_LOOP_SLEEP_MS          5

/** Loop sleep time in milliseconds when screen is blanked. */
#define PHULJHARI_BLANKED_SLEEP_MS       20

/** Total number of rows in the Settings menu. */
#define PHULJHARI_SETTINGS_ROW_COUNT     12

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
