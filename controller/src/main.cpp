/**
 * @file main.cpp
 * @brief Firmware entrypoint for Project Phuljhari M5StickS3 Controller ✨
 * 
 * Hardware: M5StickS3 (ESP32-S3-PICO-1-N8R8, ST7789P3 135x240, BMI270 IMU)
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
    Serial.println(" Project Phuljhari ✨ M5StickS3 Controller Firmware");
    Serial.println(" Tiny sparks. Wireless smiles.");
    Serial.println(" Hardware: M5StickS3 | Framework: M5Unified");
    Serial.println("========================================================");

    uint32_t start_ms = millis();

    // 1. Initialize Display & Double-Buffered Canvas
    s_ui.begin();

    // 2. Play celebratory startup animation
    s_ui.show_startup_animation(2000);

    // 3. Initialize Controller State Machine
    s_csm.init(millis());

    // 4. Initialize ESP-NOW Radio
    s_radio.begin();

    // 5. Initialize BMI270 6-Axis IMU
    s_imu.begin();

    Serial.printf("[SYSTEM] Controller boot completed in %lu ms.\n", millis() - start_ms);
}

void loop() {
    uint32_t now = millis();

    // 1. Update M5 hardware states (buttons, power, display)
    M5.update();

    // 2. Physical Button B: Cycle character theme
    if (M5.BtnB.wasClicked()) {
        s_csm.cycle_character_forward();
    }

    // 3. Physical Button A: Live press/hold synchronization & effect trigger
    if (M5.BtnA.wasPressed()) {
        s_csm.on_button_a_down(now);
    }
    if (M5.BtnA.wasReleased()) {
        s_csm.on_button_a_up(now);
    }

    // 4. Update IMU motion & deliberate gesture detection
    s_imu.update(s_csm, now);

    // 5. Advance controller state machine (bounded retries, sync refreshes, toasts)
    s_csm.tick(now);

    // 6. Pump radio queue (broadcast outgoing actions, drain incoming ACKs)
    s_radio.update(s_csm, now);

    // 7. Render UI frame onto double-buffered canvas (~35 FPS rate-limited)
    s_ui.render(s_csm, now);

    // Yield to FreeRTOS background scheduler
    yield();
}
