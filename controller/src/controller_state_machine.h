/**
 * @file controller_state_machine.h
 * @brief Hardware-independent controller state machine for M5StickS3 Controller ✨
 * 
 * Manages:
 * - Character/theme selection carousel (Phuljhari, Anar, Patakha)
 * - Live button hold synchronization (BUTTON_DOWN, BUTTON_UP, periodic BUTTON_SYNC)
 * - Effect command dispatch, sequence numbering, and bounded retries
 * - Application-level ACK matching and celebratory confirmation toasts
 * - Heartbeat-driven link health monitoring (CONNECTED, RECONNECTING, OFFLINE)
 * - Separation between virtual simulator controls and wireless command queues
 * - Zero hardware dependencies: 100% testable natively with Clang/GCC
 */

#ifndef PHULJHARI_CONTROLLER_STATE_MACHINE_H
#define PHULJHARI_CONTROLLER_STATE_MACHINE_H

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "controller_config.h"

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

class ControllerStateMachine {
public:
    ControllerStateMachine();

    void init(uint32_t current_time_ms);
    void tick(uint32_t current_time_ms);

    /* Character Selection */
    void cycle_character_forward();
    void cycle_character_backward();
    void set_selected_character(protocol::EffectId effect);
    protocol::EffectId get_selected_character() const { return selected_effect_; }

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
    protocol::LinkHealth get_link_health(uint32_t current_time_ms) const {
        return link_tracker_.evaluate(current_time_ms);
    }
    bool is_ack_toast_active(uint32_t current_time_ms) const;
    bool is_command_pending() const { return has_pending_cmd_; }
    uint8_t get_pending_retries_left() const { return retries_left_; }

private:
    void queue_action(const OutgoingAction& action);
    void trigger_effect_command(uint32_t current_time_ms);

    protocol::EffectId selected_effect_;
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

    /* ACK Confirmation Toast */
    bool ack_toast_active_;
    uint32_t ack_toast_start_ms_;

    /* Outgoing Action Mailbox */
    bool action_pending_;
    OutgoingAction current_action_;

    /* Heartbeat link health tracker */
    protocol::LinkHealthTracker link_tracker_;

    /* Virtual Simulator state (decoupled from radio commands) */
    float sim_tilt_x_;
    float sim_tilt_y_;
    bool sim_virtual_burst_;
};

} // namespace controller
} // namespace phuljhari

#endif /* PHULJHARI_CONTROLLER_STATE_MACHINE_H */
