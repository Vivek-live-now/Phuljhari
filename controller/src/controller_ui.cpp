/**
 * @file controller_ui.cpp
 * @brief Implementation of double-buffered controller UI and display overlays.
 */

#include "controller_ui.h"

namespace phuljhari {
namespace controller {

ControllerUi::ControllerUi()
    : canvas_(&M5.Display),
      last_render_ms_(0) {}

bool ControllerUi::begin() {
    Serial.println("[UI] Initializing M5StickS3 display and double-buffered canvas...");

    M5.Display.setRotation(PHULJHARI_DISP_ROTATION);
    M5.Display.setBrightness(128); // Comfortable indoor brightness

    // Create 16-bit color canvas matching full 240x135 resolution
    canvas_.setColorDepth(16);
    void* buffer = canvas_.createSprite(PHULJHARI_DISP_WIDTH, PHULJHARI_DISP_HEIGHT);
    if (buffer == nullptr) {
        Serial.println("[UI] ERROR: Failed to allocate offscreen sprite buffer!");
        return false;
    }

    anim_engine_.init();
    Serial.printf("[UI] Double-buffered canvas allocated (%dx%d, 16-bit).\n",
                  PHULJHARI_DISP_WIDTH, PHULJHARI_DISP_HEIGHT);
    return true;
}

void ControllerUi::show_startup_animation(uint32_t duration_ms) {
    uint32_t start = millis();
    uint32_t step = 0;
    while (millis() - start < duration_ms) {
        anim_engine_.render_startup_screen(canvas_, step++);
        canvas_.pushSprite(0, 0);
        delay(25);
    }
}

void ControllerUi::render(ControllerStateMachine& csm, uint32_t current_time_ms) {
    // Non-blocking frame limiter (~35 FPS / 28ms)
    if (current_time_ms - last_render_ms_ < PHULJHARI_UI_FRAME_INTERVAL_MS) {
        return;
    }
    last_render_ms_ = current_time_ms;

    // 1. Clear background
    canvas_.fillScreen(TFT_BLACK);

    // 2. Render Top Status Bar
    render_status_bar(csm, current_time_ms);

    // 3. Render Animated Character & Particles
    bool v_burst = csm.consume_sim_virtual_burst();
    anim_engine_.render_character(
        canvas_,
        csm.get_selected_character(),
        csm.get_sim_tilt_x(),
        csm.get_sim_tilt_y(),
        v_burst,
        current_time_ms
    );

    // 4. Render Bottom Footer / Status Overlay
    render_footer_bar(csm, current_time_ms);

    // 5. Push double-buffer to physical LCD with zero tearing
    canvas_.pushSprite(0, 0);
}

void ControllerUi::render_status_bar(ControllerStateMachine& csm, uint32_t current_time_ms) {
    canvas_.fillRect(0, 0, PHULJHARI_DISP_WIDTH, 18, 0x18C3); // Dark slate header
    canvas_.drawLine(0, 18, PHULJHARI_DISP_WIDTH, 18, 0x3186);

    // A. Link Health Indicator
    protocol::LinkHealth health = csm.get_link_health(current_time_ms);
    uint16_t link_color = TFT_RED;
    const char* link_txt = "OFFLINE";

    if (health == protocol::LINK_CONNECTED) {
        link_color = TFT_GREEN;
        link_txt = "LINK OK";
    } else if (health == protocol::LINK_RECONNECTING) {
        link_color = TFT_YELLOW;
        link_txt = "RETRY";
    }

    canvas_.fillCircle(8, 9, 3, link_color);
    canvas_.setTextDatum(middle_left);
    canvas_.setTextSize(1);
    canvas_.setTextColor(link_color);
    canvas_.drawString(link_txt, 15, 9);

    // B. Event Counter
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(TFT_WHITE);
    char evt_buf[16];
    snprintf(evt_buf, sizeof(evt_buf), "#%lu", (unsigned long)csm.get_total_triggers());
    canvas_.drawString(evt_buf, PHULJHARI_DISP_WIDTH / 2, 9);

    // C. Battery Gauge
    int battery_pct = M5.Power.getBatteryLevel();
    if (battery_pct < 0) battery_pct = 100; // Power plugged in / charging

    uint16_t batt_color = (battery_pct > 20) ? TFT_GREEN : TFT_RED;
    char batt_buf[16];
    snprintf(batt_buf, sizeof(batt_buf), "%d%%", battery_pct);

    canvas_.setTextDatum(middle_right);
    canvas_.setTextColor(batt_color);
    canvas_.drawString(batt_buf, PHULJHARI_DISP_WIDTH - 18, 9);
    // Draw miniature battery outline
    canvas_.drawRect(PHULJHARI_DISP_WIDTH - 14, 5, 10, 8, batt_color);
    canvas_.drawPixel(PHULJHARI_DISP_WIDTH - 4, 8, batt_color);
}

void ControllerUi::render_footer_bar(ControllerStateMachine& csm, uint32_t current_time_ms) {
    int bar_y = PHULJHARI_DISP_HEIGHT - 20;

    // 1. Celebratory ACK Toast Overlay
    if (csm.is_ack_toast_active(current_time_ms)) {
        canvas_.fillRect(0, bar_y, PHULJHARI_DISP_WIDTH, 20, 0xFD20); // Amber banner
        canvas_.setTextDatum(middle_center);
        canvas_.setTextSize(1);
        canvas_.setTextColor(TFT_BLACK);
        canvas_.drawString("*** ACK CONFIRMED! ***", PHULJHARI_DISP_WIDTH / 2, bar_y + 10);
        return;
    }

    // 2. Command in flight
    if (csm.is_command_pending()) {
        canvas_.fillRect(0, bar_y, PHULJHARI_DISP_WIDTH, 20, 0x2124);
        canvas_.setTextDatum(middle_center);
        canvas_.setTextSize(1);
        canvas_.setTextColor(TFT_YELLOW);
        char tx_buf[32];
        snprintf(tx_buf, sizeof(tx_buf), "SENDING... (R:%d)", csm.get_pending_retries_left());
        canvas_.drawString(tx_buf, PHULJHARI_DISP_WIDTH / 2, bar_y + 10);
        return;
    }

    // 3. Live Button Hold Status
    if (csm.is_button_a_held()) {
        canvas_.fillRect(0, bar_y, PHULJHARI_DISP_WIDTH, 20, 0x03E0); // Bright green
        canvas_.setTextDatum(middle_center);
        canvas_.setTextSize(1);
        canvas_.setTextColor(TFT_BLACK);
        canvas_.drawString("HOLDING [LIVE SYNC]...", PHULJHARI_DISP_WIDTH / 2, bar_y + 10);
        return;
    }

    // 4. Standard Button Hints
    canvas_.fillRect(0, bar_y, PHULJHARI_DISP_WIDTH, 20, 0x10A2); // Subtle footer
    canvas_.setTextDatum(middle_center);
    canvas_.setTextSize(1);
    canvas_.setTextColor(0xBDD7); // Light gray
    canvas_.drawString("BtnB: Cycle   |   BtnA: Spark!", PHULJHARI_DISP_WIDTH / 2, bar_y + 10);
}

} // namespace controller
} // namespace phuljhari
