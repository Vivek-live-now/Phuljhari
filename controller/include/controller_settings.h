/**
 * @file controller_settings.h
 * @brief Configurable user settings, persistence, and safe defaults for Phuljhari 2.0 ✨
 */

#ifndef PHULJHARI_CONTROLLER_SETTINGS_H
#define PHULJHARI_CONTROLLER_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "controller_config.h"

namespace phuljhari {
namespace controller {

enum BackgroundTheme : uint8_t {
    BG_MOONLIT_ROOFTOP = 0,
    BG_DIYA_COURTYARD  = 1,
    BG_FESTIVE_BAZAAR  = 2,
    BG_THEME_COUNT     = 3
};

enum FlickSensitivity : uint8_t {
    FLICK_SENS_LOW  = 0,
    FLICK_SENS_MED  = 1,
    FLICK_SENS_HIGH = 2,
    FLICK_SENS_COUNT= 3
};

struct ControllerSettings {
    bool wireless_enabled;
    bool sound_enabled;
    uint8_t volume_percent;         /**< 0 to 100 in steps of 25 */
    bool tilt_enabled;
    bool flick_enabled;
    FlickSensitivity flick_sensitivity;
    bool power_save_mode;
    BackgroundTheme background_theme;
    bool rangoli_enabled;
    uint8_t checksum;

    static inline ControllerSettings get_defaults() {
        ControllerSettings s;
        s.wireless_enabled  = true;
        s.sound_enabled     = true;
        s.volume_percent    = 75;
        s.tilt_enabled      = true;
        s.flick_enabled     = true;
        s.flick_sensitivity = FLICK_SENS_MED;
        s.power_save_mode   = false;
        s.background_theme  = BG_MOONLIT_ROOFTOP;
        s.rangoli_enabled   = true;
        s.checksum          = s.compute_checksum();
        return s;
    }

    uint8_t compute_checksum() const {
        uint8_t sum = 0x5A;
        sum ^= (wireless_enabled ? 0x01 : 0x00);
        sum ^= (sound_enabled ? 0x02 : 0x00);
        sum ^= volume_percent;
        sum ^= (tilt_enabled ? 0x04 : 0x00);
        sum ^= (flick_enabled ? 0x08 : 0x00);
        sum ^= ((uint8_t)flick_sensitivity << 4);
        sum ^= (power_save_mode ? 0x10 : 0x00);
        sum ^= ((uint8_t)background_theme << 2);
        sum ^= (rangoli_enabled ? 0x20 : 0x00);
        return sum;
    }

    bool is_valid() const {
        if (volume_percent > 100) return false;
        if (flick_sensitivity >= FLICK_SENS_COUNT) return false;
        if (background_theme >= BG_THEME_COUNT) return false;
        return (checksum == compute_checksum());
    }

    float get_flick_threshold_g() const {
        switch (flick_sensitivity) {
            case FLICK_SENS_LOW:  return PHULJHARI_FLICK_SENS_LOW_G;
            case FLICK_SENS_HIGH: return PHULJHARI_FLICK_SENS_HIGH_G;
            case FLICK_SENS_MED:
            default:              return PHULJHARI_FLICK_SENS_MED_G;
        }
    }
};

class SettingsManager {
public:
    SettingsManager() : settings_(ControllerSettings::get_defaults()) {}

    void init() {
        load();
    }

    const ControllerSettings& get() const { return settings_; }
    ControllerSettings& get_mutable() { return settings_; }

    void set(const ControllerSettings& s) {
        settings_ = s;
        settings_.checksum = settings_.compute_checksum();
        save();
    }

    void load();
    void save();

private:
    ControllerSettings settings_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_SETTINGS_H */
