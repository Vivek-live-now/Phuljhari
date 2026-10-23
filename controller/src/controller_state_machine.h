/**
 * @file controller_state_machine.h
 * @brief Hardware-independent controller state machine for Phuljhari 2.0 ✨
 * 
 * Features:
 * - 8 Selectable Festive Characters with explicit IDLE -> PLAYING -> COOLDOWN lifecycle
 * - Deliberate flick-to-start interaction & button fallbacks
 * - Sound manager integration & volume controls
 * - Settings manager with non-resetting screen navigation
 * - Wireless toggle (suspend/resume ESP-NOW dispatch cleanly)
 * - Strict GPIO 2 button synchronization preservation (DOWN, 180ms SYNC, UP)
 * - 100% testable natively with Clang/GCC
 */

#ifndef PHULJHARI_CONTROLLER_STATE_MACHINE_H
#define PHULJHARI_CONTROLLER_STATE_MACHINE_H

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "controller_config.h"
#include "character_types.h"
#include "controller_settings.h"
#include "controller_sound.h"

namespace phuljhari {
namespace controller {

enum OutgoingActionType : uint8_t {
    ACTION_NONE         = 0,
    ACTION_SEND_CMD     = 1,
    ACTION_SEND_BTN_DOWN= 2,
    ACTION_SEND_BTN_UP  = 3,
    ACTION_SEND_BTN_SYNC= 4,
    ACTION_SEND_PING    = 5
};

struct OutgoingAction {
    OutgoingActionType type;
    uint16_t seq;
    uint8_t buffer[PHULJHARI_MAX_FRAME_SIZE];
    size_t length;
};

enum ScreenMode : uint8_t {
    SCREEN_FESTIVAL = 0,
    SCREEN_SETTINGS = 1
};

class ControllerStateMachine {
public:
    ControllerStateMachine();

    void init(uint32_t current_time_ms);
    void tick(uint32_t current_time_ms);

    /* Character Selection (8 Characters) */
    void cycle_character_forward();
    void cycle_character_backward();
    void set_selected_character_id(CharacterId id);
    void set_selected_character(protocol::EffectId effect); // Backwards compatibility helper
    CharacterId get_selected_character_id() const { return selected_character_; }
    protocol::EffectId get_selected_character() const;

    /* Animation Lifecycle (IDLE -> PLAYING -> COOLDOWN -> IDLE) */
    AnimationState get_animation_state() const { return anim_state_; }
    bool trigger_animation(uint32_t current_time_ms);
    uint32_t get_anim_elapsed_ms(uint32_t current_time_ms) const;
    uint32_t get_anim_duration_ms() const { return anim_duration_ms_; }

    /* Screen Mode & Settings Navigation */
    ScreenMode get_screen_mode() const { return screen_mode_; }
    void enter_settings();
    void exit_settings();
    void toggle_settings();
    void settings_next_item();
    void settings_toggle_current();
    int get_settings_cursor() const { return settings_cursor_; }

    /* Settings & Sound Access */
    SettingsManager& get_settings_manager() { return settings_mgr_; }
    const ControllerSettings& get_settings() const { return settings_mgr_.get(); }
    ControllerSoundManager& get_sound_manager() { return sound_mgr_; }

    /* Button A Interactions (Trigger & Live Sync) */
    void on_button_a_down(uint32_t current_time_ms);
    void on_button_a_up(uint32_t current_time_ms);
    bool is_button_a_held() const { return button_a_held_; }

    /* Outgoing Action Dispatch Queue */
    bool has_pending_action() const { return action_pending_; }
    OutgoingAction pop_pending_action();

    /* Incoming Radio Packet Ingestion */
    protocol::StatusCode process_incoming_packet(
        const uint8_t* in_buf,
        size_t in_len,
        uint32_t current_time_ms
    );

    /* Virtual Simulator Control (Separated from Radio Commands) */
    void set_sim_tilt(float tilt_x, float tilt_y);
    float get_sim_tilt_x() const { return sim_tilt_x_; }
    float get_sim_tilt_y() const { return sim_tilt_y_; }
    void trigger_sim_virtual_burst() { sim_virtual_burst_ = true; }
    bool consume_sim_virtual_burst() {
        bool b = sim_virtual_burst_;
        sim_virtual_burst_ = false;
        return b;
    }

    /* State Queries */
    uint32_t get_total_triggers() const { return total_triggers_sent_; }
    uint16_t get_next_sequence_number() const { return next_seq_; }
    protocol::LinkHealth get_link_health(uint32_t current_time_ms) const;
    bool is_ack_toast_active(uint32_t current_time_ms) const;
    bool is_command_pending() const { return has_pending_cmd_; }
    uint8_t get_pending_retries_left() const { return retries_left_; }

private:
    void queue_action(const OutgoingAction& action);
    void dispatch_wireless_command(uint32_t current_time_ms);

    CharacterId selected_character_;
    AnimationState anim_state_;
    uint32_t anim_start_ms_;
    uint32_t anim_duration_ms_;
    uint32_t cooldown_start_ms_;

    ScreenMode screen_mode_;
    int settings_cursor_;

    SettingsManager settings_mgr_;
    ControllerSoundManager sound_mgr_;

    uint32_t total_triggers_sent_;
    uint16_t next_seq_;

    /* Live button hold tracking */
    bool button_a_held_;
    uint32_t button_a_pressed_ms_;
    uint32_t last_button_sync_ms_;

    /* Reliable command dispatch & bounded retries */
    bool has_pending_cmd_;
    uint16_t pending_cmd_seq_;
    protocol::EffectCmd pending_cmd_;
    uint8_t retries_left_;
    uint32_t last_tx_attempt_ms_;

    /* ACK toast display tracking */
    bool has_active_ack_;
    uint32_t ack_received_ms_;

    /* Heartbeat link vitality tracking */
    protocol::LinkHealthTracker link_tracker_;
    uint32_t last_ping_sent_ms_;

    /* Action queue */
    bool action_pending_;
    OutgoingAction pending_action_;

    /* Virtual simulator controls */
    float sim_tilt_x_;
    float sim_tilt_y_;
    bool sim_virtual_burst_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_STATE_MACHINE_H */
