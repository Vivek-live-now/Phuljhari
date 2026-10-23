/**
 * @file controller_ui.h
 * @brief Double-buffered UI renderer and status overlay for M5StickS3 Controller ✨
 */

#ifndef PHULJHARI_CONTROLLER_UI_H
#define PHULJHARI_CONTROLLER_UI_H

#include <stdint.h>
#include <M5Unified.h>

#include "controller_config.h"
#include "controller_state_machine.h"
#include "anim_characters.h"

namespace phuljhari {
namespace controller {

class ControllerUi {
public:
    ControllerUi();

    /**
     * @brief Allocates the 240x135 offscreen sprite canvas and initializes display.
     */
    bool begin();

    /**
     * @brief Shows the startup celebratory animation.
     */
    void show_startup_animation(uint32_t duration_ms);

    /**
     * @brief Non-blocking render tick. Renders frame at ~35 FPS into double buffer.
     */
    void render(ControllerStateMachine& csm, uint32_t current_time_ms);

private:
    void render_status_bar(ControllerStateMachine& csm, uint32_t current_time_ms);
    void render_footer_bar(ControllerStateMachine& csm, uint32_t current_time_ms);

    M5Canvas canvas_;
    AnimCharacterEngine anim_engine_;
    uint32_t last_render_ms_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_UI_H */
