/**
 * @file controller_settings.cpp
 * @brief Implementation of settings persistence for Phuljhari 2.0 ✨
 */

#include "controller_settings.h"

#if defined(ARDUINO) && defined(ESP32)
#include <Arduino.h>
#include <Preferences.h>

namespace phuljhari {
namespace controller {

static const char* PREF_NAMESPACE = "phuljhari";
static const char* PREF_KEY = "settings_v2";

void SettingsManager::load() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, true)) {
        settings_ = ControllerSettings::get_defaults();
        return;
    }

    ControllerSettings loaded;
    size_t len = prefs.getBytes(PREF_KEY, &loaded, sizeof(ControllerSettings));
    prefs.end();

    if (len == sizeof(ControllerSettings) && loaded.is_valid()) {
        settings_ = loaded;
    } else {
        settings_ = ControllerSettings::get_defaults();
        save();
    }
}

void SettingsManager::save() {
    Preferences prefs;
    if (prefs.begin(PREF_NAMESPACE, false)) {
        settings_.checksum = settings_.compute_checksum();
        prefs.putBytes(PREF_KEY, &settings_, sizeof(ControllerSettings));
        prefs.end();
    }
}

} // namespace controller
} // namespace phuljhari

#else

namespace phuljhari {
namespace controller {

void SettingsManager::load() {
    if (!settings_.is_valid()) {
        settings_ = ControllerSettings::get_defaults();
    }
}

void SettingsManager::save() {
    settings_.checksum = settings_.compute_checksum();
}

} // namespace controller
} // namespace phuljhari

#endif
