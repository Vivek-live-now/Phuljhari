/**
 * @file main.cpp
 * @brief Firmware entrypoint for Phuljhari 2.0 M5StickS3 Portrait Controller ✨
 * 
 * Hardware: M5StickS3 (ESP32-S3-PICO-1-N8R8, ST7789P3 135x240 Portrait, BMI270 IMU)
 * Tagline:  Tiny sparks. Wireless smiles.
 */

#include <Arduino.h>
#include <M5Unified.h>

#include "controller_config.h"
#include "controller_state_machine.h"
#include "controller_radio.h"
#include "controller_imu.h"
#include "controller_ui.h"

using namespace phuljhari;
using namespace phuljhari::controller;

static ControllerStateMachine s_csm;
static ControllerRadio s_radio;
static ControllerImu s_imu;
static ControllerUi s_ui;

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n========================================================");
    Serial.println(" Project Phuljhari 2.0 ✨ Pocket Diwali Festival");
    Serial.println(" Tiny sparks. Wireless smiles.");
    Serial.println(" Hardware: M5StickS3 | Orientation: Portrait (135x240)");
    Serial.println("========================================================");

    uint32_t start_ms = millis();

    // 1. Initialize Display & Double-Buffered Portrait Canvas (135x240)
    s_ui.begin();

    // 2. Play festive startup splash animation
    s_ui.show_startup_animation(2000);

    // 3. Initialize Controller State Machine (Settings, Sound, Characters)
    s_csm.init(millis());

    // 4. Initialize ESP-NOW Radio (Broadcast / Peer)
    s_radio.begin();

    // 5. Initialize BMI270 6-Axis IMU (Subtle tilt & Flick detector)
    s_imu.begin();

    Serial.printf("[SYSTEM] Phuljhari 2.0 boot completed in %lu ms.\n", millis() - start_ms);
}

void loop() {
    uint32_t now = millis();

    // 1. Update M5 hardware states (buttons, battery, display)
    M5.update();

    // 2. Physical Button B (Side Button):
    // - In Festival Stage: Click = Cycle Character, Hold (> 600ms) = Enter Settings
    // - In Settings Screen: Click = Next Item, Hold = Back to Festival
    if (s_csm.get_screen_mode() == SCREEN_SETTINGS) {
        if (M5.BtnB.wasHold()) {
            s_csm.exit_settings();
        } else if (M5.BtnB.wasClicked()) {
            s_csm.settings_next_item();
        }
    } else {
        if (M5.BtnB.wasHold()) {
            s_csm.enter_settings();
        } else if (M5.BtnB.wasClicked()) {
            s_csm.cycle_character_forward();
        }
    }

    // 3. Physical Button A (Front Thumb Button):
    // - In Festival Stage: Press = Hold Sync Start, Release = Hold Sync End (Short tap = Celebrate)
    // - In Settings Screen: Click = Toggle / Adjust Selected Setting
    if (s_csm.get_screen_mode() == SCREEN_SETTINGS) {
        if (M5.BtnA.wasClicked()) {
            s_csm.settings_toggle_current();
        }
    } else {
        if (M5.BtnA.wasPressed()) {
            s_csm.on_button_a_down(now);
        }
        if (M5.BtnA.wasReleased()) {
            s_csm.on_button_a_up(now);
        }
    }

    // 4. Update IMU motion & deliberate flick-to-start gesture detection
    s_imu.update(s_csm, now);

    // 5. Advance controller state machine (lifecycle, retries, sound sequencer)
    s_csm.tick(now);

    // 6. Pump radio queue (broadcast outgoing actions, receive ACKs)
    s_radio.update(s_csm, now);

    // 7. Render UI frame onto double-buffered canvas (non-blocking rate limiter)
    s_ui.render(s_csm, now);

    // Yield to FreeRTOS background scheduler
    yield();
}
