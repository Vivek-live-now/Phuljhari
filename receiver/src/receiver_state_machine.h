/**
 * @file receiver_state_machine.h
 * @brief Hardware-independent state machine for Project Phuljhari Receiver ✨
 * 
 * Manages:
 * - SAFE_IDLE, EFFECT_ACTIVE, and TIMEOUT_COOLDOWN states
 * - Input packet parsing and strict safety boundary validation
 * - Sequence deduplication and idempotent retransmission handling
 * - Automatic failsafe cutoff timer and watchdog
 * - Zero dependencies on ESP-IDF/Arduino for 100% native testability
 */

#ifndef PHULJHARI_RECEIVER_STATE_MACHINE_H
#define PHULJHARI_RECEIVER_STATE_MACHINE_H

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"
#include "receiver_config.h"

namespace phuljhari {
namespace receiver {

class ReceiverStateMachine {
public:
    ReceiverStateMachine();

    /**
     * @brief Initializes the receiver state machine.
     * Guaranteed safe: sets outputs to inactive and state to SAFE_IDLE.
     */
    void init(uint32_t current_time_ms);

    /**
     * @brief Periodic non-blocking tick.
     * Evaluates timers, state transitions, and watchdog failsafe.
     */
    void tick(uint32_t current_time_ms);

    /**
     * @brief Processes an incoming raw frame and prepares any needed response frame.
     * @param in_buf Raw incoming bytes.
     * @param in_len Length of raw incoming bytes.
     * @param current_time_ms Current time timestamp.
     * @param out_resp_buf Destination buffer for response frame (e.g. ACK or PONG).
     * @param resp_buf_capacity Capacity of response buffer.
     * @param out_resp_len Length of generated response frame.
     * @param out_has_resp Set to true if a response was generated and must be transmitted.
     * @return StatusCode result of frame processing.
     */
    protocol::StatusCode process_incoming_packet(
        const uint8_t* in_buf,
        size_t in_len,
        uint32_t current_time_ms,
        uint8_t* out_resp_buf,
        size_t resp_buf_capacity,
        size_t* out_resp_len,
        bool* out_has_resp
    );

    /**
     * @brief Direct handler for MSG_TYPE_EFFECT_CMD.
     */
    protocol::StatusCode handle_effect_cmd(
        uint16_t seq,
        const protocol::EffectCmd& cmd,
        uint32_t current_time_ms,
        protocol::EffectAck* out_ack
    );

    /**
     * @brief Direct handler for MSG_TYPE_EFFECT_STOP.
     */
    protocol::StatusCode handle_effect_stop(
        uint16_t seq,
        const protocol::EffectStopMsg& stop_msg,
        uint32_t current_time_ms
    );

    /**
     * @brief Direct handler for MSG_TYPE_PING.
     */
    void handle_ping(
        uint16_t seq,
        const protocol::PingMsg& ping,
        uint32_t current_time_ms,
        protocol::PongMsg* out_pong
    );

    /**
     * @brief Direct handler for MSG_TYPE_STATUS_REQ.
     */
    void handle_status_req(
        uint16_t seq,
        uint32_t current_time_ms,
        protocol::StatusRespMsg* out_resp
    );

    /**
     * @brief Direct handler for live button synchronization (BUTTON_DOWN, UP, SYNC).
     */
    protocol::StatusCode handle_button_event(
        protocol::MessageType type,
        const protocol::ButtonEventMsg& btn_msg,
        uint32_t current_time_ms
    );

    /* State Queries */
    protocol::ReceiverState get_state() const { return current_state_; }
    protocol::EffectId get_active_effect() const { return active_effect_; }
    uint8_t get_active_intensity() const { return active_intensity_; }
    uint16_t get_remaining_ms(uint32_t current_time_ms) const;
    bool is_output_active() const { return (current_state_ == protocol::RECV_STATE_EFFECT_ACTIVE); }
    bool is_button_indicator_active() const { return button_hold_indicator_active_; }
    uint32_t get_total_effects() const { return total_effects_executed_; }
    uint32_t get_last_activity_ms() const { return last_activity_ms_; }

private:
    protocol::ReceiverState current_state_;
    protocol::EffectId active_effect_;
    uint8_t active_intensity_;
    uint32_t effect_start_ms_;
    uint16_t effect_duration_ms_;
    uint32_t cooldown_start_ms_;
    uint32_t last_activity_ms_;
    uint32_t total_effects_executed_;

    /* Live button indicator state */
    bool button_hold_indicator_active_;
    uint32_t button_hold_last_rx_ms_;

    protocol::DuplicateFilter duplicate_filter_;

    /* Cached ACK for idempotent retransmission */
    bool has_cached_ack_;
    protocol::EffectAck cached_ack_;
};

} // namespace receiver
} // namespace phuljhari

#endif /* PHULJHARI_RECEIVER_STATE_MACHINE_H */
