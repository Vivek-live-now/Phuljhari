/**
 * @file controller_state_machine.cpp
 * @brief Implementation of Phuljhari 2.0 controller state machine ✨
 */

#include "controller_state_machine.h"
#include <cstring>

namespace phuljhari {
namespace controller {

ControllerStateMachine::ControllerStateMachine()
    : shutdown_requested_(false),
      selected_character_(CHAR_PHULJHARI),
      anim_state_(CHAR_STATE_IDLE),
      anim_start_ms_(0),
      anim_duration_ms_(3500),
      cooldown_start_ms_(0),
      screen_mode_(SCREEN_FESTIVAL),
      settings_cursor_(0),
      total_triggers_sent_(0),
      next_seq_(1),
      button_a_held_(false),
      button_a_pressed_ms_(0),
      last_button_sync_ms_(0),
      has_pending_cmd_(false),
      pending_cmd_seq_(0),
      retries_left_(0),
      last_tx_attempt_ms_(0),
      has_active_ack_(false),
      ack_received_ms_(0),
      link_tracker_(PHULJHARI_PING_INTERVAL_MS, PHULJHARI_RECONNECT_TIMEOUT_MS, PHULJHARI_OFFLINE_TIMEOUT_MS),
      last_ping_sent_ms_(0),
      action_pending_(false),
      sim_tilt_x_(0.0f),
      sim_tilt_y_(0.0f),
      sim_virtual_burst_(false) {
    std::memset(&pending_cmd_, 0, sizeof(pending_cmd_));
    std::memset(&pending_action_, 0, sizeof(pending_action_));
}

void ControllerStateMachine::init(uint32_t current_time_ms) {
    shutdown_requested_ = false;
    selected_character_ = CHAR_PHULJHARI;
    anim_state_ = CHAR_STATE_IDLE;
    anim_start_ms_ = 0;
    anim_duration_ms_ = get_character_descriptor(CHAR_PHULJHARI).duration_ms;
    cooldown_start_ms_ = 0;
    screen_mode_ = SCREEN_FESTIVAL;
    settings_cursor_ = 0;

    settings_mgr_.init();
    sound_mgr_.begin();
    sound_mgr_.set_enabled(settings_mgr_.get().sound_enabled);
    sound_mgr_.set_volume(settings_mgr_.get().volume_percent);

    total_triggers_sent_ = 0;
    next_seq_ = 1;
    button_a_held_ = false;
    button_a_pressed_ms_ = 0;
    last_button_sync_ms_ = 0;
    has_pending_cmd_ = false;
    pending_cmd_seq_ = 0;
    retries_left_ = 0;
    last_tx_attempt_ms_ = 0;
    has_active_ack_ = false;
    ack_received_ms_ = 0;
    last_ping_sent_ms_ = current_time_ms;
    action_pending_ = false;
    sim_tilt_x_ = 0.0f;
    sim_tilt_y_ = 0.0f;
    sim_virtual_burst_ = false;

    link_tracker_.reset();
}

protocol::EffectId ControllerStateMachine::get_selected_character() const {
    return get_character_descriptor(selected_character_).wireless_effect;
}

void ControllerStateMachine::set_selected_character_id(CharacterId id) {
    if (id >= CHAR_COUNT) id = CHAR_PHULJHARI;
    selected_character_ = id;
    anim_state_ = CHAR_STATE_IDLE;
    anim_start_ms_ = 0;
    anim_duration_ms_ = get_character_descriptor(selected_character_).duration_ms;
    cooldown_start_ms_ = 0;
    sound_mgr_.stop();
}

void ControllerStateMachine::set_selected_character(protocol::EffectId effect) {
    // Backwards compatibility helper mapping EffectId to CharacterId
    switch (effect) {
        case protocol::EFFECT_ID_PHULJHARI: set_selected_character_id(CHAR_PHULJHARI); break;
        case protocol::EFFECT_ID_ANAR:      set_selected_character_id(CHAR_ANAR); break;
        case protocol::EFFECT_ID_PATAKHA:   set_selected_character_id(CHAR_BOMB); break;
        default:                            set_selected_character_id(CHAR_PHULJHARI); break;
    }
}

void ControllerStateMachine::cycle_character_forward() {
    selected_character_ = (CharacterId)((selected_character_ + 1) % CHAR_COUNT);
    anim_state_ = CHAR_STATE_IDLE;
    anim_start_ms_ = 0;
    anim_duration_ms_ = get_character_descriptor(selected_character_).duration_ms;
    cooldown_start_ms_ = 0;
    sound_mgr_.stop();
}

void ControllerStateMachine::cycle_character_backward() {
    selected_character_ = (CharacterId)((selected_character_ + CHAR_COUNT - 1) % CHAR_COUNT);
    anim_state_ = CHAR_STATE_IDLE;
    anim_start_ms_ = 0;
    anim_duration_ms_ = get_character_descriptor(selected_character_).duration_ms;
    cooldown_start_ms_ = 0;
    sound_mgr_.stop();
}

bool ControllerStateMachine::trigger_animation(uint32_t current_time_ms) {
    // Only idle characters can be triggered into celebratory playing state
    if (anim_state_ != CHAR_STATE_IDLE) {
        return false;
    }

    anim_state_ = CHAR_STATE_PLAYING;
    anim_start_ms_ = current_time_ms;
    anim_duration_ms_ = get_character_descriptor(selected_character_).duration_ms;

    // Trigger personality sound
    sound_mgr_.play(get_character_descriptor(selected_character_).sound_id, current_time_ms);

    // If wireless is enabled, dispatch wireless effect command
    if (settings_mgr_.get().wireless_enabled) {
        dispatch_wireless_command(current_time_ms);
    }

    return true;
}

uint32_t ControllerStateMachine::get_anim_elapsed_ms(uint32_t current_time_ms) const {
    if (anim_state_ == CHAR_STATE_IDLE) return 0;
    if (current_time_ms < anim_start_ms_) return 0;
    return (current_time_ms - anim_start_ms_);
}

void ControllerStateMachine::enter_settings() {
    screen_mode_ = SCREEN_SETTINGS;
    settings_cursor_ = 0;
    sound_mgr_.play_ui_click(anim_start_ms_);
}

void ControllerStateMachine::exit_settings() {
    screen_mode_ = SCREEN_FESTIVAL;
    sound_mgr_.play_ui_click(anim_start_ms_);
}

void ControllerStateMachine::toggle_settings() {
    if (screen_mode_ == SCREEN_SETTINGS) {
        exit_settings();
    } else {
        enter_settings();
    }
}

void ControllerStateMachine::settings_next_item() {
    settings_cursor_ = (settings_cursor_ + 1) % PHULJHARI_SETTINGS_ROW_COUNT;
    sound_mgr_.play_ui_click(anim_start_ms_);
}

void ControllerStateMachine::settings_toggle_current() {
    ControllerSettings s = settings_mgr_.get();
    switch (settings_cursor_) {
        case 0: // Wireless ON / OFF
            s.wireless_enabled = !s.wireless_enabled;
            if (!s.wireless_enabled) {
                link_tracker_.reset();
                has_pending_cmd_ = false;
            }
            break;

        case 1: // Sound ON / OFF
            s.sound_enabled = !s.sound_enabled;
            sound_mgr_.set_enabled(s.sound_enabled);
            break;

        case 2: // Volume 0% -> 25% -> 50% -> 75% -> 100% -> 0%
            s.volume_percent = (s.volume_percent + 25);
            if (s.volume_percent > 100) s.volume_percent = 0;
            sound_mgr_.set_volume(s.volume_percent);
            break;

        case 3: // Brightness LOW -> MED -> HIGH -> LOW
            s.brightness_level = (BrightnessLevel)((s.brightness_level + 1) % BRIGHT_COUNT);
            break;

        case 4: // Tilt ON / OFF
            s.tilt_enabled = !s.tilt_enabled;
            break;

        case 5: // Flick ON / OFF
            s.flick_enabled = !s.flick_enabled;
            break;

        case 6: // Flick Sensitivity
            s.flick_sensitivity = (FlickSensitivity)((s.flick_sensitivity + 1) % FLICK_SENS_COUNT);
            break;

        case 7: // Power Save Mode
            s.power_save_mode = !s.power_save_mode;
            break;

        case 8: // Background Theme
            s.background_theme = (BackgroundTheme)((s.background_theme + 1) % BG_THEME_COUNT);
            break;

        case 9: // Rangoli Floor Art Layer
            s.rangoli_enabled = !s.rangoli_enabled;
            break;

        case 10: // [ POWER OFF ]
            shutdown_requested_ = true;
            exit_settings();
            return;

        case 11: // [ < BACK TO FESTIVAL ]
            exit_settings();
            return;

        default:
            break;
    }

    settings_mgr_.set(s);
    sound_mgr_.play_ui_click(anim_start_ms_);
}

void ControllerStateMachine::tick(uint32_t current_time_ms) {
    // 1. Advance non-blocking sound manager
    sound_mgr_.update(current_time_ms);

    // 2. Animation Lifecycle Progression (PLAYING -> COOLDOWN -> IDLE)
    if (anim_state_ == CHAR_STATE_PLAYING) {
        uint32_t elapsed = current_time_ms - anim_start_ms_;
        if (elapsed >= anim_duration_ms_) {
            anim_state_ = CHAR_STATE_COOLDOWN;
            cooldown_start_ms_ = anim_start_ms_ + anim_duration_ms_;
            sound_mgr_.stop();
        }
    }
    if (anim_state_ == CHAR_STATE_COOLDOWN) {
        uint32_t elapsed_cooldown = current_time_ms - cooldown_start_ms_;
        if (elapsed_cooldown >= PHULJHARI_ANIM_COOLDOWN_MS) {
            anim_state_ = CHAR_STATE_IDLE;
        }
    }

    // 3. Live Button Hold Periodic Sync Refresh (180ms)
    if (button_a_held_) {
        if (current_time_ms - last_button_sync_ms_ >= PHULJHARI_BUTTON_SYNC_REFRESH_MS) {
            last_button_sync_ms_ = current_time_ms;
            if (settings_mgr_.get().wireless_enabled) {
                OutgoingAction act;
                act.type = ACTION_SEND_BTN_SYNC;
                act.seq = next_seq_++;
                if (next_seq_ == 0) next_seq_ = 1;
                protocol::ButtonEventMsg msg = { 0, 0, (uint16_t)(current_time_ms - button_a_pressed_ms_) };
                protocol::serialize_button_event(
                    protocol::MSG_TYPE_BUTTON_SYNC,
                    act.seq,
                    &msg,
                    act.buffer,
                    sizeof(act.buffer),
                    &act.length
                );
                queue_action(act);
            }
        }
    }

    // 4. Reliable Effect Command Retransmission with Bounded Retries (200ms)
    if (settings_mgr_.get().wireless_enabled && has_pending_cmd_) {
        if (current_time_ms - last_tx_attempt_ms_ >= PHULJHARI_RETRY_INTERVAL_MS) {
            if (retries_left_ > 0) {
                retries_left_--;
                last_tx_attempt_ms_ = current_time_ms;
                OutgoingAction act;
                act.type = ACTION_SEND_CMD;
                act.seq = pending_cmd_seq_;
                protocol::serialize_effect_cmd(
                    act.seq,
                    &pending_cmd_,
                    act.buffer,
                    sizeof(act.buffer),
                    &act.length
                );
                queue_action(act);
            } else {
                has_pending_cmd_ = false;
            }
        }
    }

    // 5. Periodic Heartbeat Ping (Every 2000ms when idle)
    if (settings_mgr_.get().wireless_enabled) {
        if (!has_pending_cmd_ && !button_a_held_) {
            if (current_time_ms - last_ping_sent_ms_ >= PHULJHARI_PING_INTERVAL_MS) {
                last_ping_sent_ms_ = current_time_ms;
                OutgoingAction act;
                act.type = ACTION_SEND_PING;
                act.seq = next_seq_++;
                if (next_seq_ == 0) next_seq_ = 1;
                protocol::PingMsg ping = { current_time_ms };
                protocol::serialize_ping(
                    act.seq,
                    &ping,
                    act.buffer,
                    sizeof(act.buffer),
                    &act.length
                );
                queue_action(act);
            }
        }
    }
}

void ControllerStateMachine::on_button_a_down(uint32_t current_time_ms) {
    if (screen_mode_ == SCREEN_SETTINGS) {
        settings_toggle_current();
        return;
    }

    button_a_held_ = true;
    button_a_pressed_ms_ = current_time_ms;
    last_button_sync_ms_ = current_time_ms;

    if (settings_mgr_.get().wireless_enabled) {
        OutgoingAction act;
        act.type = ACTION_SEND_BTN_DOWN;
        act.seq = next_seq_++;
        if (next_seq_ == 0) next_seq_ = 1;
        protocol::ButtonEventMsg msg = { 0, 0, 0 };
        protocol::serialize_button_event(
            protocol::MSG_TYPE_BUTTON_DOWN,
            act.seq,
            &msg,
            act.buffer,
            sizeof(act.buffer),
            &act.length
        );
        queue_action(act);
    }
}

void ControllerStateMachine::on_button_a_up(uint32_t current_time_ms) {
    if (screen_mode_ == SCREEN_SETTINGS) {
        return;
    }

    if (button_a_held_) {
        button_a_held_ = false;
        uint32_t press_duration = current_time_ms - button_a_pressed_ms_;

        if (settings_mgr_.get().wireless_enabled) {
            OutgoingAction act;
            act.type = ACTION_SEND_BTN_UP;
            act.seq = next_seq_++;
            if (next_seq_ == 0) next_seq_ = 1;
            protocol::ButtonEventMsg msg = { 0, 0, (uint16_t)press_duration };
            protocol::serialize_button_event(
                protocol::MSG_TYPE_BUTTON_UP,
                act.seq,
                &msg,
                act.buffer,
                sizeof(act.buffer),
                &act.length
            );
            queue_action(act);
        }

        // Short tap (< 300ms) triggers celebration if in idle
        if (press_duration < 300) {
            trigger_animation(current_time_ms);
        }
    }
}

void ControllerStateMachine::dispatch_wireless_command(uint32_t current_time_ms) {
    total_triggers_sent_++;
    uint16_t seq = next_seq_++;
    if (next_seq_ == 0) next_seq_ = 1;

    pending_cmd_seq_ = seq;
    pending_cmd_.effect_id = get_character_descriptor(selected_character_).wireless_effect;
    pending_cmd_.duration_ms = anim_duration_ms_;
    pending_cmd_.intensity = 80;
    pending_cmd_.flags = 0;

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

void ControllerStateMachine::queue_action(const OutgoingAction& action) {
    pending_action_ = action;
    action_pending_ = true;
}

OutgoingAction ControllerStateMachine::pop_pending_action() {
    action_pending_ = false;
    return pending_action_;
}

protocol::StatusCode ControllerStateMachine::process_incoming_packet(
    const uint8_t* in_buf,
    size_t in_len,
    uint32_t current_time_ms
) {
    if (!settings_mgr_.get().wireless_enabled) {
        return protocol::STATUS_ERR_BUSY;
    }

    protocol::RawFrame frame;
    protocol::StatusCode st = protocol::deserialize_frame(in_buf, in_len, &frame);
    if (st != protocol::STATUS_OK) {
        return st;
    }

    link_tracker_.record_rx(current_time_ms);

    if (frame.msg_type == protocol::MSG_TYPE_EFFECT_ACK) {
        protocol::EffectAck ack;
        st = protocol::deserialize_effect_ack(&frame, &ack);
        if (st == protocol::STATUS_OK) {
            if (has_pending_cmd_ && ack.ack_seq == pending_cmd_seq_) {
                has_pending_cmd_ = false;
                has_active_ack_ = true;
                ack_received_ms_ = current_time_ms;
            }
        }
    } else if (frame.msg_type == protocol::MSG_TYPE_PONG) {
        protocol::PongMsg pong;
        st = protocol::deserialize_pong(&frame, &pong);
    }

    return protocol::STATUS_OK;
}

void ControllerStateMachine::set_sim_tilt(float tilt_x, float tilt_y) {
    if (settings_mgr_.get().tilt_enabled) {
        sim_tilt_x_ = tilt_x;
        sim_tilt_y_ = tilt_y;
    } else {
        sim_tilt_x_ = 0.0f;
        sim_tilt_y_ = 0.0f;
    }
}

protocol::LinkHealth ControllerStateMachine::get_link_health(uint32_t current_time_ms) const {
    if (!settings_mgr_.get().wireless_enabled) {
        return protocol::LINK_OFFLINE;
    }
    return link_tracker_.evaluate(current_time_ms);
}

bool ControllerStateMachine::is_ack_toast_active(uint32_t current_time_ms) const {
    if (!has_active_ack_) return false;
    return (current_time_ms - ack_received_ms_ < PHULJHARI_ACK_TOAST_DURATION_MS);
}

} // namespace controller
} // namespace phuljhari
