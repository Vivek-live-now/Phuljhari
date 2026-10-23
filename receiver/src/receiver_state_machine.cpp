/**
 * @file receiver_state_machine.cpp
 * @brief Implementation of hardware-independent receiver state machine.
 */

#include "receiver_state_machine.h"
#include <cstring>

namespace phuljhari {
namespace receiver {

ReceiverStateMachine::ReceiverStateMachine()
    : current_state_(protocol::RECV_STATE_SAFE_IDLE),
      active_effect_(protocol::EFFECT_ID_IDLE),
      active_intensity_(0),
      effect_start_ms_(0),
      effect_duration_ms_(0),
      cooldown_start_ms_(0),
      last_activity_ms_(0),
      total_effects_executed_(0),
      has_cached_ack_(false) {
    std::memset(&cached_ack_, 0, sizeof(cached_ack_));
}

void ReceiverStateMachine::init(uint32_t current_time_ms) {
    current_state_ = protocol::RECV_STATE_SAFE_IDLE;
    active_effect_ = protocol::EFFECT_ID_IDLE;
    active_intensity_ = 0;
    effect_start_ms_ = 0;
    effect_duration_ms_ = 0;
    cooldown_start_ms_ = 0;
    last_activity_ms_ = current_time_ms;
    total_effects_executed_ = 0;
    has_cached_ack_ = false;
    duplicate_filter_.reset();
    std::memset(&cached_ack_, 0, sizeof(cached_ack_));
}

void ReceiverStateMachine::tick(uint32_t current_time_ms) {
    /* 1. Watchdog safety check:
     * If no radio activity or command has occurred within the safety timeout,
     * immediately revert outputs to inactive safe idle. */
    if (current_time_ms - last_activity_ms_ >= PHULJHARI_SAFETY_WATCHDOG_TIMEOUT_MS) {
        if (current_state_ != protocol::RECV_STATE_SAFE_IDLE) {
            current_state_ = protocol::RECV_STATE_SAFE_IDLE;
            active_effect_ = protocol::EFFECT_ID_IDLE;
            active_intensity_ = 0;
            effect_duration_ms_ = 0;
        }
        return;
    }

    /* 2. Effect Active -> Cooldown transition */
    if (current_state_ == protocol::RECV_STATE_EFFECT_ACTIVE) {
        uint32_t elapsed = current_time_ms - effect_start_ms_;
        if (elapsed >= effect_duration_ms_) {
            current_state_ = protocol::RECV_STATE_COOLDOWN;
            cooldown_start_ms_ = current_time_ms;
            active_intensity_ = 0;
        }
    }
    /* 3. Cooldown -> Safe Idle transition */
    else if (current_state_ == protocol::RECV_STATE_COOLDOWN) {
        uint32_t elapsed_cooldown = current_time_ms - cooldown_start_ms_;
        if (elapsed_cooldown >= PHULJHARI_CONFIG_COOLDOWN_MS) {
            current_state_ = protocol::RECV_STATE_SAFE_IDLE;
            active_effect_ = protocol::EFFECT_ID_IDLE;
            active_intensity_ = 0;
        }
    }
}

uint16_t ReceiverStateMachine::get_remaining_ms(uint32_t current_time_ms) const {
    if (current_state_ != protocol::RECV_STATE_EFFECT_ACTIVE) {
        return 0;
    }
    uint32_t elapsed = current_time_ms - effect_start_ms_;
    if (elapsed >= effect_duration_ms_) {
        return 0;
    }
    return (uint16_t)(effect_duration_ms_ - elapsed);
}

protocol::StatusCode ReceiverStateMachine::process_incoming_packet(
    const uint8_t* in_buf,
    size_t in_len,
    uint32_t current_time_ms,
    uint8_t* out_resp_buf,
    size_t resp_buf_capacity,
    size_t* out_resp_len,
    bool* out_has_resp
) {
    if (out_has_resp != NULL) *out_has_resp = false;
    if (out_resp_len != NULL) *out_resp_len = 0;

    /* 1. Deserialize frame and verify CRC & magic bytes */
    protocol::RawFrame frame;
    protocol::StatusCode st = protocol::deserialize_frame(in_buf, in_len, &frame);
    if (st != protocol::STATUS_OK) {
        return st;
    }

    /* Refresh activity timestamp on any valid frame */
    last_activity_ms_ = current_time_ms;

    /* 2. Dispatch based on message type */
    switch (frame.msg_type) {
        case protocol::MSG_TYPE_EFFECT_CMD: {
            protocol::EffectCmd cmd;
            protocol::StatusCode cmd_st = protocol::deserialize_effect_cmd(&frame, &cmd);
            protocol::EffectAck ack;

            if (cmd_st != protocol::STATUS_OK) {
                // Return NACK with the specific validation or safety error
                ack.ack_seq = frame.seq;
                ack.status_code = cmd_st;
                ack.receiver_state = current_state_;
                ack.remaining_ms = get_remaining_ms(current_time_ms);
            } else {
                handle_effect_cmd(frame.seq, cmd, current_time_ms, &ack);
            }

            protocol::StatusCode ser_st = protocol::serialize_effect_ack(
                frame.seq,
                &ack,
                out_resp_buf,
                resp_buf_capacity,
                out_resp_len
            );
            if (ser_st == protocol::STATUS_OK && out_has_resp != NULL) {
                *out_has_resp = true;
            }
            return cmd_st;
        }

        case protocol::MSG_TYPE_EFFECT_STOP: {
            protocol::EffectStopMsg stop_msg;
            protocol::StatusCode stop_st = protocol::deserialize_effect_stop(&frame, &stop_msg);
            if (stop_st == protocol::STATUS_OK) {
                handle_effect_stop(frame.seq, stop_msg, current_time_ms);
            }
            // Acknowledge stop command via standard EffectAck
            protocol::EffectAck ack;
            ack.ack_seq = frame.seq;
            ack.status_code = stop_st;
            ack.receiver_state = current_state_;
            ack.remaining_ms = 0;
            protocol::StatusCode ser_st = protocol::serialize_effect_ack(
                frame.seq,
                &ack,
                out_resp_buf,
                resp_buf_capacity,
                out_resp_len
            );
            if (ser_st == protocol::STATUS_OK && out_has_resp != NULL) {
                *out_has_resp = true;
            }
            return stop_st;
        }

        case protocol::MSG_TYPE_PING: {
            protocol::PingMsg ping;
            protocol::StatusCode ping_st = protocol::deserialize_ping(&frame, &ping);
            if (ping_st == protocol::STATUS_OK) {
                protocol::PongMsg pong;
                handle_ping(frame.seq, ping, current_time_ms, &pong);
                protocol::StatusCode ser_st = protocol::serialize_pong(
                    frame.seq,
                    &pong,
                    out_resp_buf,
                    resp_buf_capacity,
                    out_resp_len
                );
                if (ser_st == protocol::STATUS_OK && out_has_resp != NULL) {
                    *out_has_resp = true;
                }
            }
            return ping_st;
        }

        case protocol::MSG_TYPE_STATUS_REQ: {
            protocol::StatusRespMsg resp;
            handle_status_req(frame.seq, current_time_ms, &resp);
            protocol::StatusCode ser_st = protocol::serialize_status_resp(
                frame.seq,
                &resp,
                out_resp_buf,
                resp_buf_capacity,
                out_resp_len
            );
            if (ser_st == protocol::STATUS_OK && out_has_resp != NULL) {
                *out_has_resp = true;
            }
            return protocol::STATUS_OK;
        }

        default:
            return protocol::STATUS_ERR_INVALID_TYPE;
    }
}

protocol::StatusCode ReceiverStateMachine::handle_effect_cmd(
    uint16_t seq,
    const protocol::EffectCmd& cmd,
    uint32_t current_time_ms,
    protocol::EffectAck* out_ack
) {
    if (out_ack == NULL) return protocol::STATUS_ERR_TRUNCATED;

    /* 1. Idempotency & Duplicate Check */
    if (duplicate_filter_.check_and_record(seq)) {
        /* Sequence already seen. If it matches our last executed command,
         * resend the ACK with updated remaining time without restarting the animation. */
        if (has_cached_ack_ && cached_ack_.ack_seq == seq) {
            *out_ack = cached_ack_;
            out_ack->receiver_state = current_state_;
            out_ack->remaining_ms = get_remaining_ms(current_time_ms);
            return protocol::STATUS_OK;
        } else {
            /* Older duplicate packet outside current active command */
            out_ack->ack_seq = seq;
            out_ack->status_code = protocol::STATUS_ERR_DUPLICATE;
            out_ack->receiver_state = current_state_;
            out_ack->remaining_ms = get_remaining_ms(current_time_ms);
            return protocol::STATUS_ERR_DUPLICATE;
        }
    }

    /* 2. Validate Effect ID */
    if (!protocol::is_valid_effect_id(cmd.effect_id)) {
        out_ack->ack_seq = seq;
        out_ack->status_code = protocol::STATUS_ERR_INVALID_EFFECT;
        out_ack->receiver_state = current_state_;
        out_ack->remaining_ms = get_remaining_ms(current_time_ms);
        return protocol::STATUS_ERR_INVALID_EFFECT;
    }

    /* 3. Validate Safety Boundaries */
    if (!safety::is_duration_safe(cmd.duration_ms) || !safety::is_intensity_safe(cmd.intensity)) {
        out_ack->ack_seq = seq;
        out_ack->status_code = protocol::STATUS_ERR_SAFETY_VIOLATION;
        out_ack->receiver_state = current_state_;
        out_ack->remaining_ms = get_remaining_ms(current_time_ms);
        return protocol::STATUS_ERR_SAFETY_VIOLATION;
    }

    /* 4. Execute Command */
    if (cmd.effect_id == protocol::EFFECT_ID_IDLE) {
        current_state_ = protocol::RECV_STATE_SAFE_IDLE;
        active_effect_ = protocol::EFFECT_ID_IDLE;
        active_intensity_ = 0;
        effect_duration_ms_ = 0;
    } else {
        current_state_ = protocol::RECV_STATE_EFFECT_ACTIVE;
        active_effect_ = (protocol::EffectId)cmd.effect_id;
        active_intensity_ = safety::clamp_intensity(cmd.intensity);
        effect_start_ms_ = current_time_ms;
        effect_duration_ms_ = safety::clamp_duration(cmd.duration_ms);
        total_effects_executed_++;
    }

    /* 5. Populate and Cache ACK */
    out_ack->ack_seq = seq;
    out_ack->status_code = protocol::STATUS_OK;
    out_ack->receiver_state = current_state_;
    out_ack->remaining_ms = effect_duration_ms_;

    cached_ack_ = *out_ack;
    has_cached_ack_ = true;

    return protocol::STATUS_OK;
}

protocol::StatusCode ReceiverStateMachine::handle_effect_stop(
    uint16_t seq,
    const protocol::EffectStopMsg& /*stop_msg*/,
    uint32_t /*current_time_ms*/
) {
    (void)seq;
    current_state_ = protocol::RECV_STATE_SAFE_IDLE;
    active_effect_ = protocol::EFFECT_ID_IDLE;
    active_intensity_ = 0;
    effect_duration_ms_ = 0;
    has_cached_ack_ = false;
    return protocol::STATUS_OK;
}

void ReceiverStateMachine::handle_ping(
    uint16_t seq,
    const protocol::PingMsg& /*ping*/,
    uint32_t current_time_ms,
    protocol::PongMsg* out_pong
) {
    (void)seq;
    if (out_pong != NULL) {
        out_pong->uptime_ms = current_time_ms;
        out_pong->receiver_state = current_state_;
    }
}

void ReceiverStateMachine::handle_status_req(
    uint16_t seq,
    uint32_t current_time_ms,
    protocol::StatusRespMsg* out_resp
) {
    (void)seq;
    if (out_resp != NULL) {
        out_resp->current_effect = (uint8_t)active_effect_;
        out_resp->receiver_state = (uint8_t)current_state_;
        out_resp->remaining_duration_ms = get_remaining_ms(current_time_ms);
        out_resp->total_effects_executed = total_effects_executed_;
    }
}

} // namespace receiver
} // namespace phuljhari
