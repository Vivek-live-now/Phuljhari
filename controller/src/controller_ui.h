/**
 * @file controller_ui.h
 * @brief Double-buffered portrait display engine (135x240) and settings UI for Phuljhari 2.0 ✨
 */

#ifndef PHULJHARI_CONTROLLER_UI_H
#define PHULJHARI_CONTROLLER_UI_H

#include <stdint.h>
#include <stdbool.h>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

#include "controller_config.h"
#include "controller_state_machine.h"
#include "anim_characters.h"

namespace phuljhari {
namespace controller {

class ControllerUi {
public:
    ControllerUi();

    bool begin();
    void show_startup_animation(uint32_t duration_ms);
    void render(ControllerStateMachine& csm, uint32_t current_time_ms);

private:
#if defined(ARDUINO)
    void render_festival_screen(ControllerStateMachine& csm, uint32_t current_time_ms);
    void render_status_bar(ControllerStateMachine& csm, uint32_t current_time_ms);
    void render_footer_bar(ControllerStateMachine& csm, uint32_t current_time_ms);
    void render_ack_toast(ControllerStateMachine& csm, uint32_t current_time_ms);
    void render_settings_screen(ControllerStateMachine& csm, uint32_t current_time_ms);

    M5Canvas canvas_;
    AnimCharacterEngine anim_engine_;
#endif
    uint32_t last_render_ms_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_UI_H */
