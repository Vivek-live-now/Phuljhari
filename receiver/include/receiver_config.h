/**
 * @file receiver_config.h
 * @brief Hardware configuration and pin mappings for Project Phuljhari Receiver ✨
 * 
 * Hardware: ESP32-WROOM-32D Development Board
 * 
 * ============================================================================
 * SAFETY & HARDWARE BOUNDARIES
 * ============================================================================
 * Exclusively for low-voltage decorative LED lighting (<= 5V DC).
 * Strictly NO ignition, pyrotechnic, or high-energy actuation devices.
 * ============================================================================
 */

#ifndef PHULJHARI_RECEIVER_CONFIG_H
#define PHULJHARI_RECEIVER_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * Addressable LED Strip Configuration (WS2812B / SK6812)
 * ------------------------------------------------------------------------- */
/** Enable driving addressable RGB LED strip via FastLED. */
#define PHULJHARI_ENABLE_STRIP              1

/**
 * GPIO pin connected to the WS2812B Data In line.
 * Recommended safe pins on ESP32-WROOM-32D: GPIO 18, 19, 21, 22, 23.
 * Avoid strapping pins (GPIO 0, 2, 12, 15) and UART pins (GPIO 1, 3).
 * Default: GPIO 18 (VSPI SCK pin on standard DevKit, safe general-purpose I/O).
 */
#define PHULJHARI_CONFIG_STRIP_PIN          18

/**
 * Number of addressable RGB LEDs in the decorative strip.
 * Default: 30 LEDs (suitable for standard 0.5m / 1m demonstration strips).
 */
#define PHULJHARI_CONFIG_STRIP_NUM_LEDS     30

/** FastLED color order (typical WS2812B strips use GRB). */
#define PHULJHARI_CONFIG_STRIP_COLOR_ORDER  GRB

/* -------------------------------------------------------------------------
 * Bench-Test / Onboard Indicator LED Configuration
 * ------------------------------------------------------------------------- */
/** Enable bench-test indicator LED for zero-external-hardware testing. */
#define PHULJHARI_ENABLE_ONBOARD_LED        1

/**
 * Onboard LED GPIO Pin.
 * Standard ESP32 DevKit V1 / DOIT boards typically use GPIO 2.
 * Some NodeMCU-32S boards use GPIO 2; certain clones use GPIO 4, 5, or 16.
 * Check your board's schematic if the onboard LED does not illuminate.
 */
#define PHULJHARI_CONFIG_ONBOARD_LED_PIN    2

/**
 * Logic level that turns the onboard LED ON.
 * true  = Active HIGH (3.3V illuminates LED) - standard on most ESP32 dev boards.
 * false = Active LOW (0V illuminates LED) - used on some inverted LED designs.
 */
#define PHULJHARI_CONFIG_ONBOARD_ACTIVE_HIGH true

/* -------------------------------------------------------------------------
 * Wireless & Radio Settings
 * ------------------------------------------------------------------------- */
/** Wi-Fi Channel for ESP-NOW (must match controller). */
#define PHULJHARI_CONFIG_WIFI_CHANNEL       1

/** Maximum queued incoming packets in the radio ring buffer. */
#define PHULJHARI_CONFIG_RX_QUEUE_SIZE      8

/* -------------------------------------------------------------------------
 * Timing & State Machine Parameters
 * ------------------------------------------------------------------------- */
/** Safety cooldown duration in milliseconds after an effect finishes (300 ms). */
#define PHULJHARI_CONFIG_COOLDOWN_MS        300

/** LED frame update rate (target ~50 FPS = 20 ms per frame). */
#define PHULJHARI_CONFIG_FRAME_INTERVAL_MS  20

#endif /* PHULJHARI_RECEIVER_CONFIG_H */
