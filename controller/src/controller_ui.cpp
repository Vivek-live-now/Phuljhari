/**
 * @file controller_ui.cpp
 * @brief Implementation of portrait display engine (135x240) and settings UI for Phuljhari 2.0 ✨
 */

#include "controller_ui.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace phuljhari {
namespace controller {

ControllerUi::ControllerUi()
#if defined(ARDUINO)
    : canvas_(&M5.Display),
      last_render_ms_(0),
      last_activity_time_ms_(0),
      current_brightness_(75),
      display_dimmed_(false),
      display_blanked_(false)
#else
    : last_render_ms_(0),
      last_activity_time_ms_(0),
      current_brightness_(75),
      display_dimmed_(false),
      display_blanked_(false)
#endif
{}

bool ControllerUi::begin() {
#if defined(ARDUINO)
    Serial.println("[UI] Initializing Portrait M5StickS3 display (135x240)...");

    M5.Display.setRotation(PHULJHARI_DISP_ROTATION); // 0 = Portrait
    current_brightness_ = 75; // Balanced default (~30% brightness, cuts backlight drain)
    M5.Display.setBrightness(current_brightness_);

    canvas_.setColorDepth(16);
    void* buffer = canvas_.createSprite(PHULJHARI_DISP_WIDTH, PHULJHARI_DISP_HEIGHT);
    if (buffer == nullptr) {
        Serial.println("[UI] ERROR: Failed to allocate portrait offscreen sprite buffer!");
        return false;
    }

    anim_engine_.init();
    last_activity_time_ms_ = millis();
    display_dimmed_ = false;
    display_blanked_ = false;

    Serial.printf("[UI] Double-buffered portrait canvas allocated (%dx%d, 16-bit).\n",
                  PHULJHARI_DISP_WIDTH, PHULJHARI_DISP_HEIGHT);
    return true;
#else
    return true;
#endif
}

void ControllerUi::notify_user_activity(uint32_t current_time_ms) {
    last_activity_time_ms_ = current_time_ms;
#if defined(ARDUINO)
    if (display_blanked_ || display_dimmed_) {
        display_blanked_ = false;
        display_dimmed_ = false;
        M5.Display.setBrightness(current_brightness_);
    }
#endif
}

void ControllerUi::shutdown_power_off() {
#if defined(ARDUINO)
    Serial.println("[UI] Powering off M5StickS3...");
    canvas_.fillScreen(TFT_BLACK);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextSize(1);
    canvas_.setTextColor(0xFEA0);
    canvas_.drawString("✦ SHUTTING DOWN ✦", PHULJHARI_DISP_WIDTH / 2, PHULJHARI_DISP_HEIGHT / 2 - 12);
    canvas_.setTextColor(TFT_WHITE);
    canvas_.drawString("GOODBYE! ✨", PHULJHARI_DISP_WIDTH / 2, PHULJHARI_DISP_HEIGHT / 2 + 8);
    canvas_.pushSprite(0, 0);
    delay(500);
    M5.Display.setBrightness(0);
    delay(100);
    M5.Power.powerOff();
#endif
}

void ControllerUi::show_startup_animation(uint32_t duration_ms) {
#if defined(ARDUINO)
    uint32_t start = millis();
    uint32_t step = 0;
    while (millis() - start < duration_ms) {
        anim_engine_.render_startup_screen(canvas_, step++);
        canvas_.pushSprite(0, 0);
        delay(25);
    }
#else
    (void)duration_ms;
#endif
}

void ControllerUi::render(ControllerStateMachine& csm, uint32_t current_time_ms) {
#if defined(ARDUINO)
    // Synchronize display brightness with settings
    uint8_t target_brightness = csm.get_settings().get_brightness_value();
    if (current_brightness_ != target_brightness) {
        current_brightness_ = target_brightness;
        if (!display_dimmed_ && !display_blanked_) {
            M5.Display.setBrightness(current_brightness_);
        }
    }

    // Keep active during playing animations
    if (csm.get_anim_state() == CHAR_STATE_PLAYING) {
        last_activity_time_ms_ = current_time_ms;
    }

    // Check inactivity timeouts (dim -> blank -> auto-off)
    uint32_t inactive_ms = current_time_ms - last_activity_time_ms_;
    if (inactive_ms >= PHULJHARI_AUTO_POWER_OFF_MS) {
        shutdown_power_off();
        return;
    } else if (inactive_ms >= PHULJHARI_INACTIVITY_BLANK_MS) {
        if (!display_blanked_) {
            display_blanked_ = true;
            M5.Display.setBrightness(0);
        }
    } else if (inactive_ms >= PHULJHARI_INACTIVITY_DIM_MS) {
        if (!display_dimmed_) {
            display_dimmed_ = true;
            uint8_t dimmed = PHULJHARI_BRIGHTNESS_DIM;
            if (dimmed > current_brightness_) dimmed = current_brightness_;
            M5.Display.setBrightness(dimmed);
        }
    } else {
        if (display_dimmed_ || display_blanked_) {
            display_dimmed_ = false;
            display_blanked_ = false;
            M5.Display.setBrightness(current_brightness_);
        }
    }

    // If screen is blanked, skip heavy rendering and SPI transfer entirely
    if (display_blanked_) {
        return;
    }

    uint32_t frame_interval = csm.get_settings().power_save_mode
                                  ? PHULJHARI_UI_POWER_SAVE_INTERVAL_MS
                                  : PHULJHARI_UI_FRAME_INTERVAL_MS;

    if (current_time_ms - last_render_ms_ < frame_interval) {
        return;
    }
    last_render_ms_ = current_time_ms;

    if (csm.get_screen_mode() == SCREEN_SETTINGS) {
        render_settings_screen(csm, current_time_ms);
    } else {
        render_festival_screen(csm, current_time_ms);
    }

    // Push double-buffered sprite to physical portrait LCD with zero tearing
    canvas_.pushSprite(0, 0);
#else
    (void)csm;
    (void)current_time_ms;
#endif
}

#if defined(ARDUINO)

void ControllerUi::render_festival_screen(ControllerStateMachine& csm, uint32_t current_time_ms) {
    const ControllerSettings& cfg = csm.get_settings();

    // 1. Festive Background Theme (Rooftop / Courtyard / Bazaar)
    anim_engine_.render_background(canvas_, cfg.background_theme, cfg.power_save_mode, current_time_ms);

    // 2. Rangoli Floor Mandala Layer
    anim_engine_.render_rangoli(canvas_, cfg.rangoli_enabled, current_time_ms);

    // 3. Compact Top Status Bar (y: 0 - 20)
    render_status_bar(csm, current_time_ms);

    // 4. Large Centered Cracker Character (y: 20 - 185)
    anim_engine_.render_character(
        canvas_,
        csm.get_selected_character_id(),
        csm.get_animation_state(),
        csm.get_sim_tilt_x(),
        csm.get_sim_tilt_y(),
        csm.get_anim_elapsed_ms(current_time_ms),
        current_time_ms
    );

    // 5. Bottom Control Area & Character Info (y: 185 - 240)
    render_footer_bar(csm, current_time_ms);

    // 6. Celebratory ACK Toast Overlay
    if (csm.is_ack_toast_active(current_time_ms)) {
        render_ack_toast(csm, current_time_ms);
    }
}

void ControllerUi::render_status_bar(ControllerStateMachine& csm, uint32_t current_time_ms) {
    canvas_.fillRect(0, 0, PHULJHARI_DISP_WIDTH, 18, 0x0842); // Midnight slate bar
    canvas_.drawLine(0, 18, PHULJHARI_DISP_WIDTH, 18, 0xFEA0);

    // A. Wireless Link Status
    canvas_.setTextDatum(middle_left);
    canvas_.setTextSize(1);
    if (!csm.get_settings().wireless_enabled) {
        canvas_.fillCircle(6, 9, 2, TFT_DARKGRAY);
        canvas_.setTextColor(TFT_DARKGRAY);
        canvas_.drawString("W-OFF", 12, 9);
    } else {
        protocol::LinkHealth health = csm.get_link_health(current_time_ms);
        uint16_t link_color = (health == protocol::LINK_CONNECTED) ? TFT_GREEN :
                              (health == protocol::LINK_RECONNECTING) ? TFT_YELLOW : TFT_RED;
        canvas_.fillCircle(6, 9, 2, link_color);
        canvas_.setTextColor(link_color);
        canvas_.drawString((health == protocol::LINK_CONNECTED) ? "LINK" : "LOST", 12, 9);
    }

    // B. Sound Status
    canvas_.setTextDatum(middle_center);
    if (!csm.get_settings().sound_enabled || csm.get_settings().volume_percent == 0) {
        canvas_.setTextColor(TFT_DARKGRAY);
        canvas_.drawString("MUTE", 68, 9);
    } else {
        canvas_.setTextColor(0xFEA0); // Gold
        char vol_str[8];
        snprintf(vol_str, sizeof(vol_str), "%d%%", csm.get_settings().volume_percent);
        canvas_.drawString(vol_str, 68, 9);
    }

    // C. Real Battery Telemetry
    canvas_.setTextDatum(middle_right);
    int bat_pct = M5.Power.getBatteryLevel();
    if (bat_pct >= 0 && bat_pct <= 100) {
        canvas_.setTextColor((bat_pct > 25) ? TFT_GREEN : TFT_YELLOW);
        char bat_str[10];
        snprintf(bat_str, sizeof(bat_str), "%d%%", bat_pct);
        canvas_.drawString(bat_str, PHULJHARI_DISP_WIDTH - 4, 9);
    } else {
        canvas_.setTextColor(TFT_LIGHTGRAY);
        canvas_.drawString("USB", PHULJHARI_DISP_WIDTH - 4, 9);
    }
}

void ControllerUi::render_footer_bar(ControllerStateMachine& csm, uint32_t current_time_ms) {
    const CharacterDescriptor& desc = get_character_descriptor(csm.get_selected_character_id());
    AnimationState state = csm.get_animation_state();

    // Semi-transparent dark footer backing
    canvas_.fillRect(0, 196, PHULJHARI_DISP_WIDTH, 44, 0x0821);
    canvas_.drawLine(0, 196, PHULJHARI_DISP_WIDTH, 196, 0xFD20);

    // 1. Character Name Banner
    canvas_.setTextDatum(top_center);
    canvas_.setTextSize(1);
    canvas_.setTextColor(0xFEA0); // Festive Gold
    char banner[24];
    snprintf(banner, sizeof(banner), "* %s *", desc.display_name);
    canvas_.drawString(banner, PHULJHARI_DISP_WIDTH / 2, 199);

    // 2. Contextual Gesture Action Hint
    if (state == CHAR_STATE_IDLE) {
        // Soft pulsing prompt
        bool alt = ((current_time_ms / 1400) % 2 == 0);
        canvas_.setTextColor(0xFFDF); // Warm White
        if (alt && csm.get_settings().flick_enabled) {
            canvas_.drawString("FLICK TO CELEBRATE", PHULJHARI_DISP_WIDTH / 2, 212);
        } else {
            canvas_.drawString("TAP A TO CELEBRATE", PHULJHARI_DISP_WIDTH / 2, 212);
        }
    } else if (state == CHAR_STATE_PLAYING) {
        // Active celebration progress gauge
        canvas_.setTextColor(0x07E0); // Green
        canvas_.drawString("CELEBRATING! *", PHULJHARI_DISP_WIDTH / 2, 212);

        uint32_t elapsed = csm.get_anim_elapsed_ms(current_time_ms);
        int bar_w = 70;
        int bar_h = 3;
        int bar_x = (PHULJHARI_DISP_WIDTH - bar_w) / 2;
        int bar_y = 222;
        canvas_.drawRect(bar_x, bar_y, bar_w, bar_h, 0x3186);
        int fill_w = (int)((elapsed * (bar_w - 2)) / desc.duration_ms);
        if (fill_w > bar_w - 2) fill_w = bar_w - 2;
        canvas_.fillRect(bar_x + 1, bar_y + 1, fill_w, bar_h - 2, 0xFEA0);
    } else {
        canvas_.setTextColor(0x54BF); // Sky Blue
        canvas_.drawString("COOLDOWN...", PHULJHARI_DISP_WIDTH / 2, 212);
    }

    // 3. Settings shortcut reminder
    canvas_.setTextColor(0x52AA); // Muted slate
    canvas_.drawString("HOLD B: SETTINGS", PHULJHARI_DISP_WIDTH / 2, 228);
}

void ControllerUi::render_ack_toast(ControllerStateMachine& /*csm*/, uint32_t /*current_time_ms*/) {
    int toast_w = 90;
    int toast_h = 20;
    int toast_x = (PHULJHARI_DISP_WIDTH - toast_w) / 2;
    int toast_y = 30;

    canvas_.fillRoundRect(toast_x, toast_y, toast_w, toast_h, 4, 0x07E0);
    canvas_.drawRoundRect(toast_x, toast_y, toast_w, toast_h, 4, TFT_WHITE);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(TFT_BLACK);
    canvas_.setTextSize(1);
    canvas_.drawString("ACK RECEIVED!", PHULJHARI_DISP_WIDTH / 2, toast_y + 10);
}

void ControllerUi::render_settings_screen(ControllerStateMachine& csm, uint32_t /*current_time_ms*/) {
    canvas_.fillScreen(0x0821); // Dark indigo background

    // Header
    canvas_.fillRect(0, 0, PHULJHARI_DISP_WIDTH, 22, 0x18C3);
    canvas_.drawLine(0, 22, PHULJHARI_DISP_WIDTH, 22, 0xFEA0);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextSize(1);
    canvas_.setTextColor(0xFEA0);
    canvas_.drawString("✦ SETTINGS ✦", PHULJHARI_DISP_WIDTH / 2, 11);

    const ControllerSettings& s = csm.get_settings();
    int cursor = csm.get_settings_cursor();

    static const char* THEME_NAMES[] = { "ROOFTOP", "COURTYARD", "BAZAAR" };
    static const char* SENS_NAMES[]  = { "LOW", "MED", "HIGH" };
    static const char* BRIGHT_NAMES[] = { "LOW", "MED", "HIGH" };

    struct SettingItemView {
        const char* label;
        char value[16];
    };

    SettingItemView items[PHULJHARI_SETTINGS_ROW_COUNT];
    items[0].label = "Wireless";
    snprintf(items[0].value, 16, "%s", s.wireless_enabled ? "ON" : "OFF");

    items[1].label = "Sound";
    snprintf(items[1].value, 16, "%s", s.sound_enabled ? "ON" : "OFF");

    items[2].label = "Volume";
    snprintf(items[2].value, 16, "%d%%", s.volume_percent);

    items[3].label = "Brightness";
    snprintf(items[3].value, 16, "%s", BRIGHT_NAMES[s.brightness_level % BRIGHT_COUNT]);

    items[4].label = "Motion Tilt";
    snprintf(items[4].value, 16, "%s", s.tilt_enabled ? "ON" : "OFF");

    items[5].label = "Flick-Start";
    snprintf(items[5].value, 16, "%s", s.flick_enabled ? "ON" : "OFF");

    items[6].label = "Flick Sens";
    snprintf(items[6].value, 16, "%s", SENS_NAMES[s.flick_sensitivity % FLICK_SENS_COUNT]);

    items[7].label = "Power Save";
    snprintf(items[7].value, 16, "%s", s.power_save_mode ? "ON" : "OFF");

    items[8].label = "Theme";
    snprintf(items[8].value, 16, "%s", THEME_NAMES[s.background_theme % BG_THEME_COUNT]);

    items[9].label = "Rangoli";
    snprintf(items[9].value, 16, "%s", s.rangoli_enabled ? "ON" : "OFF");

    items[10].label = "[ POWER OFF ]";
    items[10].value[0] = '\0';

    items[11].label = "[ < BACK ]";
    items[11].value[0] = '\0';

    // Render list of 12 items
    int start_y = 24;
    int line_h = 16;

    for (int i = 0; i < PHULJHARI_SETTINGS_ROW_COUNT; ++i) {
        int y = start_y + i * line_h;
        bool is_cur = (i == cursor);

        if (is_cur) {
            uint16_t box_color = (i == 10) ? 0x8800 : 0x3186;
            uint16_t border_color = (i == 10) ? 0xF800 : 0xFEA0;
            canvas_.fillRect(2, y - 1, PHULJHARI_DISP_WIDTH - 4, line_h - 1, box_color);
            canvas_.drawRect(2, y - 1, PHULJHARI_DISP_WIDTH - 4, line_h - 1, border_color);
        }

        canvas_.setTextSize(1);

        if (i == 10) {
            canvas_.setTextDatum(middle_center);
            canvas_.setTextColor(is_cur ? 0xF800 : 0xFA20);
            canvas_.drawString("[ POWER OFF ]", PHULJHARI_DISP_WIDTH / 2, y + 6);
        } else if (i == 11) {
            canvas_.setTextDatum(middle_center);
            canvas_.setTextColor(is_cur ? 0xFEA0 : 0xFFDF);
            canvas_.drawString("[ < BACK TO STAGE ]", PHULJHARI_DISP_WIDTH / 2, y + 6);
        } else {
            canvas_.setTextDatum(middle_left);
            canvas_.setTextColor(is_cur ? 0xFEA0 : 0xFFDF);
            char row_buf[32];
            snprintf(row_buf, sizeof(row_buf), "%s%s", is_cur ? "> " : "  ", items[i].label);
            canvas_.drawString(row_buf, 4, y + 6);

            canvas_.setTextDatum(middle_right);
            canvas_.setTextColor(is_cur ? 0xFFFF : 0xFD20);
            canvas_.drawString(items[i].value, PHULJHARI_DISP_WIDTH - 8, y + 6);
        }
    }

    // Settings Navigation Controls Hint
    canvas_.fillRect(0, 218, PHULJHARI_DISP_WIDTH, 22, 0x0842);
    canvas_.drawLine(0, 218, PHULJHARI_DISP_WIDTH, 218, 0xFD20);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(0xFEA0);
    canvas_.drawString("B: NEXT  |  A: TOGGLE", PHULJHARI_DISP_WIDTH / 2, 229);
}

#endif // ARDUINO

} // namespace controller
} // namespace phuljhari
