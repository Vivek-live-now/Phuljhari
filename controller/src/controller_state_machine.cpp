/**
 * @file controller_state_machine.cpp
 * @brief Implementation of hardware-independent controller state machine.
 */

#include "controller_state_machine.h"
#include <cstring>

namespace phuljhari {
namespace controller {

ControllerStateMachine::ControllerStateMachine()
    : selected_effect_(protocol::EFFECT_ID_PHULJHARI),
      total_triggers_sent_(0),
      next_seq_(1),
      button_a_held_(false),
      button_a_pressed_ms_(0),
      last_button_sync_ms_(0),
      has_pending_cmd_(false),
      pending_cmd_seq_(0),
      retries_left_(0),
      last_tx_attempt_ms_(0),
      ack_toast_active_(false),
      ack_toast_start_ms_(0),
      action_pending_(false),
      link_tracker_(PHULJHARI_PING_INTERVAL_MS, PHULJHARI_RECONNECT_TIMEOUT_MS, PHULJHARI_OFFLINE_TIMEOUT_MS),
      sim_tilt_x_(0.0f),
      sim_tilt_y_(0.0f),
      sim_virtual_burst_(false) {
    std::memset(&pending_cmd_, 0, sizeof(pending_cmd_));
    std::memset(&current_action_, 0, sizeof(current_action_));
}

void ControllerStateMachine::init(uint32_t current_time_ms) {
    selected_effect_ = protocol::EFFECT_ID_PHULJHARI;
    total_triggers_sent_ = 0;
    next_seq_ = 1;
    button_a_held_ = false;
    button_a_pressed_ms_ = 0;
    last_button_sync_ms_ = 0;
    has_pending_cmd_ = false;
    pending_cmd_seq_ = 0;
    retries_left_ = 0;
    last_tx_attempt_ms_ = 0;
    ack_toast_active_ = false;
    ack_toast_start_ms_ = 0;
    action_pending_ = false;
    sim_tilt_x_ = 0.0f;
    sim_tilt_y_ = 0.0f;
    sim_virtual_burst_ = false;
    link_tracker_.record_ping_tx(current_time_ms);
}

void ControllerStateMachine::cycle_character_forward() {
    switch (selected_effect_) {
        case protocol::EFFECT_ID_PHULJHARI:
            selected_effect_ = protocol::EFFECT_ID_ANAR;
            break;
        case protocol::EFFECT_ID_ANAR:
            selected_effect_ = protocol::EFFECT_ID_PATAKHA;
            break;
        case protocol::EFFECT_ID_PATAKHA:
        default:
            selected_effect_ = protocol::EFFECT_ID_PHULJHARI;
            break;
    }
}

void ControllerStateMachine::cycle_character_backward() {
    switch (selected_effect_) {
        case protocol::EFFECT_ID_PHULJHARI:
            selected_effect_ = protocol::EFFECT_ID_PATAKHA;
            break;
        case protocol::EFFECT_ID_ANAR:
            selected_effect_ = protocol::EFFECT_ID_PHULJHARI;
            break;
        case protocol::EFFECT_ID_PATAKHA:
        default:
            selected_effect_ = protocol::EFFECT_ID_ANAR;
            break;
    }
}

void ControllerStateMachine::set_selected_character(protocol::EffectId effect) {
    if (effect >= protocol::EFFECT_ID_PHULJHARI && effect <= protocol::EFFECT_ID_PATAKHA) {
        selected_effect_ = effect;
    }
}

void ControllerStateMachine::queue_action(const OutgoingAction& action) {
    current_action_ = action;
    action_pending_ = true;
}

OutgoingAction ControllerStateMachine::pop_pending_action() {
    OutgoingAction act = current_action_;
    action_pending_ = false;
    return act;
}

void ControllerStateMachine::on_button_a_down(uint32_t current_time_ms) {
    if (!button_a_held_) {
        button_a_held_ = true;
        button_a_pressed_ms_ = current_time_ms;
        last_button_sync_ms_ = current_time_ms;

        // 1. Immediately send BUTTON_DOWN with dedicated sequence number
        uint16_t seq = next_seq_++;
        OutgoingAction act;
        act.type = ACTION_SEND_BTN_DOWN;
        act.seq = seq;

        protocol::ButtonEventMsg btn = { 0, 0, 0 };
        protocol::serialize_button_event(
            protocol::MSG_TYPE_BUTTON_DOWN,
            seq,
            &btn,
            act.buffer,
            sizeof(act.buffer),
            &act.length
        );
        queue_action(act);
    }
}

void ControllerStateMachine::on_button_a_up(uint32_t current_time_ms) {
    if (button_a_held_) {
        button_a_held_ = false;
        uint16_t hold_duration = (uint16_t)(current_time_ms - button_a_pressed_ms_);

        // 1. Send BUTTON_UP so receiver turns off status LED
        uint16_t btn_seq = next_seq_++;
        OutgoingAction btn_act;
        btn_act.type = ACTION_SEND_BTN_UP;
        btn_act.seq = btn_seq;

        protocol::ButtonEventMsg btn = { 0, 0, hold_duration };
        protocol::serialize_button_event(
            protocol::MSG_TYPE_BUTTON_UP,
            btn_seq,
            &btn,
            btn_act.buffer,
            sizeof(btn_act.buffer),
            &btn_act.length
        );
        queue_action(btn_act);

        // 2. Dispatch effect command for selected character
        trigger_effect_command(current_time_ms);
    }
}

void ControllerStateMachine::trigger_effect_command(uint32_t current_time_ms) {
    uint16_t seq = next_seq_++;
    total_triggers_sent_++;

    pending_cmd_.effect_id = (uint8_t)selected_effect_;
    pending_cmd_.duration_ms = PHULJHARI_SAFETY_DEFAULT_DURATION_MS;
    pending_cmd_.intensity = PHULJHARI_SAFETY_DEFAULT_INTENSITY_PCT;
    pending_cmd_.flags = 0;

    pending_cmd_seq_ = seq;
    has_pending_cmd_ = true;
    retries_left_ = PHULJHARI_MAX_TX_RETRIES;
    last_tx_attempt_ms_ = current_time_ms;

    OutgoingAction act;
    act.type = ACTION_SEND_CMD;
    act.seq = seq;
    protocol::serialize_effect_cmd(
        seq,
        &pending_cmd_,
        act.buffer,
        sizeof(act.buffer),
        &act.length
    );
    queue_action(act);
}

void ControllerStateMachine::tick(uint32_t current_time_ms) {
    // 1. Live button hold refresh (send periodic BUTTON_SYNC while button remains pressed)
    if (button_a_held_) {
        if (current_time_ms - last_button_sync_ms_ >= PHULJHARI_BUTTON_SYNC_REFRESH_MS) {
            last_button_sync_ms_ = current_time_ms;
            uint16_t seq = next_seq_++;

            OutgoingAction act;
            act.type = ACTION_SEND_BTN_SYNC;
            act.seq = seq;

            protocol::ButtonEventMsg btn = { 0, 0, (uint16_t)(current_time_ms - button_a_pressed_ms_) };
            protocol::serialize_button_event(
                protocol::MSG_TYPE_BUTTON_SYNC,
                seq,
                &btn,
                act.buffer,
                sizeof(act.buffer),
                &act.length
            );
            queue_action(act);
        }
    }

    // 2. Outgoing Command Retries with Bounded Timeout
    if (has_pending_cmd_) {
        if (current_time_ms - last_tx_attempt_ms_ >= PHULJHARI_RETRY_INTERVAL_MS) {
            last_tx_attempt_ms_ = current_time_ms;
            if (retries_left_ > 0) {
                retries_left_--;
                OutgoingAction act;
                act.type = ACTION_SEND_CMD;
                act.seq = pending_cmd_seq_;
                protocol::serialize_effect_cmd(
                    pending_cmd_seq_,
                    &pending_cmd_,
                    act.buffer,
                    sizeof(act.buffer),
                    &act.length
                );
                queue_action(act);
            } else {
                // All bounded retries exhausted without ACK
                has_pending_cmd_ = false;
            }
        }
    }

    // 3. Heartbeat PING transmission when idle
    if (!has_pending_cmd_ && !button_a_held_) {
        if (link_tracker_.should_send_ping(current_time_ms)) {
            link_tracker_.record_ping_tx(current_time_ms);
            uint16_t seq = next_seq_++;

            OutgoingAction act;
            act.type = ACTION_SEND_PING;
            act.seq = seq;

            protocol::PingMsg ping = { current_time_ms };
            protocol::serialize_ping(
                seq,
                &ping,
                act.buffer,
                sizeof(act.buffer),
                &act.length
            );
            queue_action(act);
        }
    }

    // 4. ACK Toast Expiration
    if (ack_toast_active_) {
        if (current_time_ms - ack_toast_start_ms_ >= PHULJHARI_ACK_TOAST_DURATION_MS) {
            ack_toast_active_ = false;
        }
    }
}

protocol::StatusCode ControllerStateMachine::process_incoming_packet(
    const uint8_t* in_buf,
    size_t in_len,
    uint32_t current_time_ms
) {
    protocol::RawFrame frame;
    protocol::StatusCode st = protocol::deserialize_frame(in_buf, in_len, &frame);
    if (st != protocol::STATUS_OK) {
        return st;
    }

    // Record valid packet arrival for link health
    link_tracker_.record_rx(current_time_ms);

    if (frame.msg_type == protocol::MSG_TYPE_EFFECT_ACK) {
        protocol::EffectAck ack;
        protocol::StatusCode ack_st = protocol::deserialize_effect_ack(&frame, &ack);
        if (ack_st == protocol::STATUS_OK) {
            // Confirm matching pending sequence
            if (has_pending_cmd_ && ack.ack_seq == pending_cmd_seq_) {
                has_pending_cmd_ = false;
                ack_toast_active_ = true;
                ack_toast_start_ms_ = current_time_ms;
            }
        }
        return ack_st;
    } else if (frame.msg_type == protocol::MSG_TYPE_PONG) {
        return protocol::STATUS_OK;
    }

    return protocol::STATUS_OK;
}

bool ControllerStateMachine::is_ack_toast_active(uint32_t current_time_ms) const {
    if (!ack_toast_active_) return false;
    return (current_time_ms - ack_toast_start_ms_ < PHULJHARI_ACK_TOAST_DURATION_MS);
}

void ControllerStateMachine::set_sim_tilt(float tilt_x, float tilt_y) {
    sim_tilt_x_ = tilt_x;
    sim_tilt_y_ = tilt_y;
}

} // namespace controller
} // namespace phuljhari
