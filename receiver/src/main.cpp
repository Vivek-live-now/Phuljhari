/**
 * @file main.cpp
 * @brief Firmware entrypoint for Project Phuljhari Receiver ✨
 * 
 * Hardware: ESP32-WROOM-32D Development Board
 * Tagline:  Tiny sparks. Wireless smiles.
 * 
 * ============================================================================
 * SAFETY & HARDWARE BOUNDARY POLICY
 * ============================================================================
 * Exclusively for harmless decorative lighting and low-voltage loads (<= 5V DC).
 * Strictly NO pyrotechnic or ignition device interfacing.
 * Outputs default to inactive (LOW) state.
 * ============================================================================
 */

#include <Arduino.h>
#include "receiver_config.h"
#include "receiver_state_machine.h"
#include "receiver_radio.h"
#include "led_controller.h"

using namespace phuljhari;
using namespace phuljhari::receiver;

static ReceiverStateMachine s_state_machine;
static ReceiverRadio s_radio(s_state_machine);
static LedController s_leds;

void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n========================================================");
    Serial.println(" Project Phuljhari ✨ Receiver Firmware");
    Serial.println(" Tiny sparks. Wireless smiles.");
    Serial.println(" Hardware: ESP32-WROOM-32D | Protocol: ESP-NOW");
    Serial.println(" Safety Policy: Exclusively Decorative LED Outputs");
    Serial.println("========================================================");

    uint32_t start_ms = millis();

    // 1. Initialize State Machine with safe idle defaults
    s_state_machine.init(start_ms);

    // 2. Initialize LED Controller (FastLED & Onboard Indicator)
    s_leds.begin();

    // 3. Initialize ESP-NOW Radio
    if (!s_radio.begin()) {
        Serial.println("[ERROR] Radio initialization failed! Reverting to safe idle.");
    }

    Serial.printf("[SYSTEM] Boot completed in %lu ms. Awaiting commands...\n", millis() - start_ms);
}

void loop() {
    uint32_t now = millis();

    // 1. Drain queued packets from radio ISR and process them
    s_radio.update(now);

    // 2. Advance receiver state machine (timers, transitions, watchdog failsafe)
    s_state_machine.tick(now);

    // 3. Render decorative LED frame (non-blocking, ~50 FPS rate-limited)
    s_leds.update(
        s_state_machine.get_state(),
        s_state_machine.get_active_effect(),
        s_state_machine.get_active_intensity(),
        s_state_machine.is_button_indicator_active(),
        now
    );

    // Yield to FreeRTOS background tasks
    yield();
}
