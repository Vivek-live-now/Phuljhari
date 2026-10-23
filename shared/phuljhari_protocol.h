/**
 * @file phuljhari_protocol.h
 * @brief Wire protocol, message definitions, serialization, CRC, and sequence logic
 *        for Project Phuljhari ✨
 * 
 * Standalone C++ header: zero dependencies on Arduino or ESP-IDF.
 * Fully compilable on host systems (Clang / GCC) for native unit testing.
 */

#ifndef PHULJHARI_PROTOCOL_H
#define PHULJHARI_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "phuljhari_safety.h"

#ifdef __cplusplus
namespace phuljhari {
namespace protocol {
#endif

/* -------------------------------------------------------------------------
 * Protocol Framing Constants
 * ------------------------------------------------------------------------- */
#define PHULJHARI_MAGIC_0               ((uint8_t)0x50) /* 'P' */
#define PHULJHARI_MAGIC_1               ((uint8_t)0x48) /* 'H' */
#define PHULJHARI_PROTO_VERSION         ((uint8_t)0x01)

/** Header size: Magic(2) + Version(1) + MsgType(1) + Seq(2) + PayloadLen(1) = 7 */
#define PHULJHARI_HEADER_SIZE           ((size_t)7)
/** Trailer size: CRC8(1) = 1 */
#define PHULJHARI_TRAILER_SIZE          ((size_t)1)
/** Total frame overhead: Header + CRC8 = 8 */
#define PHULJHARI_FRAME_OVERHEAD        (PHULJHARI_HEADER_SIZE + PHULJHARI_TRAILER_SIZE)

/** Maximum payload bytes per application message. */
#define PHULJHARI_MAX_PAYLOAD_SIZE      ((size_t)32)
/** Absolute maximum wire frame size (Header + Max Payload + CRC8). */
#define PHULJHARI_MAX_FRAME_SIZE        (PHULJHARI_FRAME_OVERHEAD + PHULJHARI_MAX_PAYLOAD_SIZE)
/** Minimum wire frame size (Empty payload + Header + CRC8). */
#define PHULJHARI_MIN_FRAME_SIZE        (PHULJHARI_FRAME_OVERHEAD)

/** Default ESP-NOW Wi-Fi Channel for direct peer communication. */
#define PHULJHARI_DEFAULT_CHANNEL       ((uint8_t)1)

/* -------------------------------------------------------------------------
 * Message Types
 * ------------------------------------------------------------------------- */
enum MessageType : uint8_t {
    MSG_TYPE_PING        = 0x01, /**< Heartbeat ping from controller */
    MSG_TYPE_PONG        = 0x02, /**< Heartbeat pong response from receiver */
    MSG_TYPE_EFFECT_CMD  = 0x03, /**< Command to trigger decorative lighting effect */
    MSG_TYPE_EFFECT_ACK  = 0x04, /**< Application-level confirmation of effect command */
    MSG_TYPE_EFFECT_STOP = 0x05, /**< Immediate stop command reverting to safe idle */
    MSG_TYPE_STATUS_REQ  = 0x06, /**< Query receiver state and telemetry */
    MSG_TYPE_STATUS_RESP = 0x07  /**< Receiver state and telemetry report */
};

/* -------------------------------------------------------------------------
 * Decorative Effect Identifiers
 * ------------------------------------------------------------------------- */
enum EffectId : uint8_t {
    EFFECT_ID_IDLE       = 0x00, /**< Safe idle: low ambient twinkle or blackout */
    EFFECT_ID_PHULJHARI  = 0x01, /**< Golden sparkles & gentle twinkling effect */
    EFFECT_ID_ANAR       = 0x02, /**< Colorful fountain particle animation effect */
    EFFECT_ID_PATAKHA    = 0x03, /**< Cute cartoon pixel-burst animation effect */
    EFFECT_ID_MAX_VALID  = 0x03
};

/* -------------------------------------------------------------------------
 * Receiver State Machine States
 * ------------------------------------------------------------------------- */
enum ReceiverState : uint8_t {
    RECV_STATE_SAFE_IDLE      = 0x00, /**< Outputs inactive / gentle ambient */
    RECV_STATE_EFFECT_ACTIVE  = 0x01, /**< Running decorative LED effect */
    RECV_STATE_COOLDOWN       = 0x02, /**< Post-effect safety pause */
    RECV_STATE_ERROR_RECOVERY = 0x03  /**< Inactive output during recovery */
};

/* -------------------------------------------------------------------------
 * Protocol Status & Error Codes
 * ------------------------------------------------------------------------- */
enum StatusCode : uint8_t {
    STATUS_OK                   = 0x00, /**< Success */
    STATUS_ERR_MAGIC            = 0x01, /**< Invalid magic bytes */
    STATUS_ERR_VERSION          = 0x02, /**< Protocol version mismatch */
    STATUS_ERR_CRC              = 0x03, /**< CRC-8 checksum verification failed */
    STATUS_ERR_TRUNCATED        = 0x04, /**< Frame shorter than header or declared payload */
    STATUS_ERR_INVALID_TYPE     = 0x05, /**< Unknown or unsupported message type */
    STATUS_ERR_INVALID_EFFECT   = 0x06, /**< Unrecognized decorative effect ID */
    STATUS_ERR_SAFETY_VIOLATION = 0x07, /**< Duration or intensity exceeds safety limit */
    STATUS_ERR_DUPLICATE        = 0x08, /**< Sequence number recognized as duplicate */
    STATUS_ERR_BUFFER_OVERFLOW  = 0x09, /**< Destination buffer too small for serialization */
    STATUS_ERR_BUSY             = 0x0A, /**< Receiver busy, cannot accept command */
    STATUS_ERR_PAYLOAD_LENGTH   = 0x0B  /**< Payload length incorrect for message type */
};

/* -------------------------------------------------------------------------
 * Transport, Application, and Link Separation
 * ------------------------------------------------------------------------- */
/**
 * @brief Transport-layer transmission status.
 * Reflects radio driver / MAC callback status, NOT application receipt.
 */
enum TransportStatus : uint8_t {
    TRANSPORT_TX_IDLE    = 0x00,
    TRANSPORT_TX_SUCCESS = 0x01, /**< Radio MAC ACK received by transceiver */
    TRANSPORT_TX_FAIL    = 0x02  /**< Radio MAC send timed out / peer missing */
};

/**
 * @brief Link health state determined by heartbeats and application ACKs.
 */
enum LinkHealth : uint8_t {
    LINK_CONNECTED       = 0x00, /**< Active bidirectional communication */
    LINK_RECONNECTING    = 0x01, /**< Missed heartbeat or retrying packet */
    LINK_OFFLINE         = 0x02  /**< Link dead / receiver unreachable */
};

/* -------------------------------------------------------------------------
 * Explicit Network Byte Order (Big-Endian) Helpers
 * ------------------------------------------------------------------------- */
static inline void write_u16_be(uint8_t* dst, uint16_t val) {
    dst[0] = (uint8_t)((val >> 8) & 0xFF);
    dst[1] = (uint8_t)(val & 0xFF);
}

static inline uint16_t read_u16_be(const uint8_t* src) {
    return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

static inline void write_u32_be(uint8_t* dst, uint32_t val) {
    dst[0] = (uint8_t)((val >> 24) & 0xFF);
    dst[1] = (uint8_t)((val >> 16) & 0xFF);
    dst[2] = (uint8_t)((val >> 8) & 0xFF);
    dst[3] = (uint8_t)(val & 0xFF);
}

static inline uint32_t read_u32_be(const uint8_t* src) {
    return (((uint32_t)src[0] << 24) |
            ((uint32_t)src[1] << 16) |
            ((uint32_t)src[2] << 8)  |
            ((uint32_t)src[3]));
}

/* -------------------------------------------------------------------------
 * CRC-8 Implementation (CCITT polynomial 0x07, init 0x00)
 * ------------------------------------------------------------------------- */
static inline uint8_t crc8_update(uint8_t crc, uint8_t data) {
    crc ^= data;
    for (uint8_t i = 0; i < 8; ++i) {
        if (crc & 0x80) {
            crc = (uint8_t)((crc << 1) ^ 0x07);
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static inline uint8_t crc8_calculate(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    if (data == NULL) return 0;
    for (size_t i = 0; i < length; ++i) {
        crc = crc8_update(crc, data[i]);
    }
    return crc;
}

/* -------------------------------------------------------------------------
 * Unmarshaled Application Message Data Structures
 * ------------------------------------------------------------------------- */
struct RawFrame {
    uint8_t magic[2];
    uint8_t version;
    uint8_t msg_type;
    uint16_t seq;
    uint8_t payload_len;
    uint8_t payload[PHULJHARI_MAX_PAYLOAD_SIZE];
    uint8_t crc;
};

struct EffectCmd {
    uint8_t effect_id;     /**< EffectId (EFFECT_ID_PHULJHARI, ANAR, PATAKHA) */
    uint16_t duration_ms;  /**< Milliseconds (bounded by safety policy) */
    uint8_t intensity;     /**< Intensity percentage 1-100% */
    uint8_t flags;         /**< Reserved flags (default 0) */
};

#define PHULJHARI_PAYLOAD_LEN_EFFECT_CMD ((uint8_t)5)

struct EffectAck {
    uint16_t ack_seq;        /**< Sequence number being acknowledged */
    uint8_t status_code;     /**< StatusCode result */
    uint8_t receiver_state;  /**< ReceiverState enum */
    uint16_t remaining_ms;   /**< Remaining effect runtime in ms */
};

#define PHULJHARI_PAYLOAD_LEN_EFFECT_ACK ((uint8_t)6)

struct PingMsg {
    uint32_t uptime_ms; /**< Sender uptime timestamp */
};

#define PHULJHARI_PAYLOAD_LEN_PING ((uint8_t)4)

struct PongMsg {
    uint32_t uptime_ms;     /**< Receiver uptime timestamp */
    uint8_t receiver_state; /**< ReceiverState enum */
};

#define PHULJHARI_PAYLOAD_LEN_PONG ((uint8_t)5)

struct EffectStopMsg {
    uint8_t reason; /**< 0 = User requested, 1 = Timeout, 2 = Safety cutoff */
};

#define PHULJHARI_PAYLOAD_LEN_EFFECT_STOP ((uint8_t)1)

struct StatusRespMsg {
    uint8_t current_effect;          /**< Currently active EffectId */
    uint8_t receiver_state;          /**< ReceiverState enum */
    uint16_t remaining_duration_ms;  /**< Milliseconds left */
    uint32_t total_effects_executed; /**< Telemetry counter */
};

#define PHULJHARI_PAYLOAD_LEN_STATUS_RESP ((uint8_t)8)

/* -------------------------------------------------------------------------
 * Validation Helpers
 * ------------------------------------------------------------------------- */
static inline bool is_valid_message_type(uint8_t msg_type) {
    return (msg_type >= MSG_TYPE_PING && msg_type <= MSG_TYPE_STATUS_RESP);
}

static inline bool is_valid_effect_id(uint8_t effect_id) {
    return (effect_id <= EFFECT_ID_MAX_VALID);
}

/* -------------------------------------------------------------------------
 * Explicit Byte Serialization and Deserialization
 * ------------------------------------------------------------------------- */

/**
 * @brief Serializes a generic frame into raw bytes with explicit field layout and CRC.
 */
static inline StatusCode serialize_frame(
    uint8_t msg_type,
    uint16_t seq,
    const uint8_t* payload,
    uint8_t payload_len,
    uint8_t* out_buf,
    size_t out_buf_capacity,
    size_t* out_frame_len
) {
    if (out_buf == NULL || out_frame_len == NULL) {
        return STATUS_ERR_BUFFER_OVERFLOW;
    }
    if (!is_valid_message_type(msg_type)) {
        return STATUS_ERR_INVALID_TYPE;
    }
    if (payload_len > PHULJHARI_MAX_PAYLOAD_SIZE) {
        return STATUS_ERR_BUFFER_OVERFLOW;
    }
    size_t total_len = PHULJHARI_FRAME_OVERHEAD + payload_len;
    if (out_buf_capacity < total_len) {
        return STATUS_ERR_BUFFER_OVERFLOW;
    }

    /* Header fields */
    out_buf[0] = PHULJHARI_MAGIC_0;
    out_buf[1] = PHULJHARI_MAGIC_1;
    out_buf[2] = PHULJHARI_PROTO_VERSION;
    out_buf[3] = msg_type;
    write_u16_be(&out_buf[4], seq);
    out_buf[6] = payload_len;

    /* Payload fields */
    if (payload_len > 0 && payload != NULL) {
        for (size_t i = 0; i < payload_len; ++i) {
            out_buf[PHULJHARI_HEADER_SIZE + i] = payload[i];
        }
    }

    /* CRC-8 covers entire header + payload */
    size_t data_to_hash = PHULJHARI_HEADER_SIZE + payload_len;
    out_buf[data_to_hash] = crc8_calculate(out_buf, data_to_hash);

    *out_frame_len = total_len;
    return STATUS_OK;
}

/**
 * @brief Deserializes a raw buffer into an unmarshaled RawFrame with full integrity checks.
 */
static inline StatusCode deserialize_frame(
    const uint8_t* in_buf,
    size_t in_buf_len,
    RawFrame* out_frame
) {
    if (in_buf == NULL || out_frame == NULL) {
        return STATUS_ERR_TRUNCATED;
    }
    if (in_buf_len < PHULJHARI_MIN_FRAME_SIZE) {
        return STATUS_ERR_TRUNCATED;
    }

    /* 1. Magic check */
    if (in_buf[0] != PHULJHARI_MAGIC_0 || in_buf[1] != PHULJHARI_MAGIC_1) {
        return STATUS_ERR_MAGIC;
    }

    /* 2. Version check */
    if (in_buf[2] != PHULJHARI_PROTO_VERSION) {
        return STATUS_ERR_VERSION;
    }

    /* 3. Message type check */
    uint8_t msg_type = in_buf[3];
    if (!is_valid_message_type(msg_type)) {
        return STATUS_ERR_INVALID_TYPE;
    }

    /* 4. Sequence number */
    uint16_t seq = read_u16_be(&in_buf[4]);

    /* 5. Payload length and truncation checks */
    uint8_t payload_len = in_buf[6];
    if (payload_len > PHULJHARI_MAX_PAYLOAD_SIZE) {
        return STATUS_ERR_PAYLOAD_LENGTH;
    }
    size_t expected_total_len = PHULJHARI_FRAME_OVERHEAD + payload_len;
    if (in_buf_len < expected_total_len) {
        return STATUS_ERR_TRUNCATED;
    }

    /* 6. CRC-8 integrity verification */
    size_t data_len = PHULJHARI_HEADER_SIZE + payload_len;
    uint8_t calculated_crc = crc8_calculate(in_buf, data_len);
    uint8_t received_crc = in_buf[data_len];
    if (calculated_crc != received_crc) {
        return STATUS_ERR_CRC;
    }

    /* Populate output frame */
    out_frame->magic[0] = in_buf[0];
    out_frame->magic[1] = in_buf[1];
    out_frame->version = in_buf[2];
    out_frame->msg_type = msg_type;
    out_frame->seq = seq;
    out_frame->payload_len = payload_len;
    for (size_t i = 0; i < payload_len; ++i) {
        out_frame->payload[i] = in_buf[PHULJHARI_HEADER_SIZE + i];
    }
    out_frame->crc = received_crc;

    return STATUS_OK;
}

/* -------------------------------------------------------------------------
 * Typed Message Serializers & Deserializers
 * ------------------------------------------------------------------------- */

/**
 * @brief Serializes MSG_TYPE_EFFECT_CMD.
 * Enforces hardware safety limits on duration and intensity before serialization.
 */
static inline StatusCode serialize_effect_cmd(
    uint16_t seq,
    const EffectCmd* cmd,
    uint8_t* out_buf,
    size_t out_buf_capacity,
    size_t* out_frame_len
) {
    if (cmd == NULL) return STATUS_ERR_TRUNCATED;
    if (!is_valid_effect_id(cmd->effect_id)) {
        return STATUS_ERR_INVALID_EFFECT;
    }
    if (!safety::is_duration_safe(cmd->duration_ms) || !safety::is_intensity_safe(cmd->intensity)) {
        return STATUS_ERR_SAFETY_VIOLATION;
    }

    uint8_t payload[PHULJHARI_PAYLOAD_LEN_EFFECT_CMD];
    payload[0] = cmd->effect_id;
    write_u16_be(&payload[1], cmd->duration_ms);
    payload[3] = cmd->intensity;
    payload[4] = cmd->flags;

    return serialize_frame(
        MSG_TYPE_EFFECT_CMD,
        seq,
        payload,
        PHULJHARI_PAYLOAD_LEN_EFFECT_CMD,
        out_buf,
        out_buf_capacity,
        out_frame_len
    );
}

/**
 * @brief Deserializes MSG_TYPE_EFFECT_CMD from RawFrame with safety validation.
 */
static inline StatusCode deserialize_effect_cmd(
    const RawFrame* frame,
    EffectCmd* out_cmd
) {
    if (frame == NULL || out_cmd == NULL) return STATUS_ERR_TRUNCATED;
    if (frame->msg_type != MSG_TYPE_EFFECT_CMD) return STATUS_ERR_INVALID_TYPE;
    if (frame->payload_len != PHULJHARI_PAYLOAD_LEN_EFFECT_CMD) return STATUS_ERR_PAYLOAD_LENGTH;

    uint8_t effect_id = frame->payload[0];
    if (!is_valid_effect_id(effect_id)) {
        return STATUS_ERR_INVALID_EFFECT;
    }

    uint16_t duration = read_u16_be(&frame->payload[1]);
    uint8_t intensity = frame->payload[3];
    uint8_t flags = frame->payload[4];

    if (!safety::is_duration_safe(duration) || !safety::is_intensity_safe(intensity)) {
        return STATUS_ERR_SAFETY_VIOLATION;
    }

    out_cmd->effect_id = effect_id;
    out_cmd->duration_ms = duration;
    out_cmd->intensity = intensity;
    out_cmd->flags = flags;
    return STATUS_OK;
}

/**
 * @brief Serializes MSG_TYPE_EFFECT_ACK.
 */
static inline StatusCode serialize_effect_ack(
    uint16_t seq,
    const EffectAck* ack,
    uint8_t* out_buf,
    size_t out_buf_capacity,
    size_t* out_frame_len
) {
    if (ack == NULL) return STATUS_ERR_TRUNCATED;
    uint8_t payload[PHULJHARI_PAYLOAD_LEN_EFFECT_ACK];
    write_u16_be(&payload[0], ack->ack_seq);
    payload[2] = ack->status_code;
    payload[3] = ack->receiver_state;
    write_u16_be(&payload[4], ack->remaining_ms);

    return serialize_frame(
        MSG_TYPE_EFFECT_ACK,
        seq,
        payload,
        PHULJHARI_PAYLOAD_LEN_EFFECT_ACK,
        out_buf,
        out_buf_capacity,
        out_frame_len
    );
}

/**
 * @brief Deserializes MSG_TYPE_EFFECT_ACK.
 */
static inline StatusCode deserialize_effect_ack(
    const RawFrame* frame,
    EffectAck* out_ack
) {
    if (frame == NULL || out_ack == NULL) return STATUS_ERR_TRUNCATED;
    if (frame->msg_type != MSG_TYPE_EFFECT_ACK) return STATUS_ERR_INVALID_TYPE;
    if (frame->payload_len != PHULJHARI_PAYLOAD_LEN_EFFECT_ACK) return STATUS_ERR_PAYLOAD_LENGTH;

    out_ack->ack_seq = read_u16_be(&frame->payload[0]);
    out_ack->status_code = frame->payload[2];
    out_ack->receiver_state = frame->payload[3];
    out_ack->remaining_ms = read_u16_be(&frame->payload[4]);
    return STATUS_OK;
}

/**
 * @brief Serializes MSG_TYPE_PING.
 */
static inline StatusCode serialize_ping(
    uint16_t seq,
    const PingMsg* ping,
    uint8_t* out_buf,
    size_t out_buf_capacity,
    size_t* out_frame_len
) {
    if (ping == NULL) return STATUS_ERR_TRUNCATED;
    uint8_t payload[PHULJHARI_PAYLOAD_LEN_PING];
    write_u32_be(&payload[0], ping->uptime_ms);

    return serialize_frame(
        MSG_TYPE_PING,
        seq,
        payload,
        PHULJHARI_PAYLOAD_LEN_PING,
        out_buf,
        out_buf_capacity,
        out_frame_len
    );
}

/**
 * @brief Deserializes MSG_TYPE_PING.
 */
static inline StatusCode deserialize_ping(
    const RawFrame* frame,
    PingMsg* out_ping
) {
    if (frame == NULL || out_ping == NULL) return STATUS_ERR_TRUNCATED;
    if (frame->msg_type != MSG_TYPE_PING) return STATUS_ERR_INVALID_TYPE;
    if (frame->payload_len != PHULJHARI_PAYLOAD_LEN_PING) return STATUS_ERR_PAYLOAD_LENGTH;

    out_ping->uptime_ms = read_u32_be(&frame->payload[0]);
    return STATUS_OK;
}

/**
 * @brief Serializes MSG_TYPE_PONG.
 */
static inline StatusCode serialize_pong(
    uint16_t seq,
    const PongMsg* pong,
    uint8_t* out_buf,
    size_t out_buf_capacity,
    size_t* out_frame_len
) {
    if (pong == NULL) return STATUS_ERR_TRUNCATED;
    uint8_t payload[PHULJHARI_PAYLOAD_LEN_PONG];
    write_u32_be(&payload[0], pong->uptime_ms);
    payload[4] = pong->receiver_state;

    return serialize_frame(
        MSG_TYPE_PONG,
        seq,
        payload,
        PHULJHARI_PAYLOAD_LEN_PONG,
        out_buf,
        out_buf_capacity,
        out_frame_len
    );
}

/**
 * @brief Deserializes MSG_TYPE_PONG.
 */
static inline StatusCode deserialize_pong(
    const RawFrame* frame,
    PongMsg* out_pong
) {
    if (frame == NULL || out_pong == NULL) return STATUS_ERR_TRUNCATED;
    if (frame->msg_type != MSG_TYPE_PONG) return STATUS_ERR_INVALID_TYPE;
    if (frame->payload_len != PHULJHARI_PAYLOAD_LEN_PONG) return STATUS_ERR_PAYLOAD_LENGTH;

    out_pong->uptime_ms = read_u32_be(&frame->payload[0]);
    out_pong->receiver_state = frame->payload[4];
    return STATUS_OK;
}

/**
 * @brief Serializes MSG_TYPE_EFFECT_STOP.
 */
static inline StatusCode serialize_effect_stop(
    uint16_t seq,
    const EffectStopMsg* stop,
    uint8_t* out_buf,
    size_t out_buf_capacity,
    size_t* out_frame_len
) {
    if (stop == NULL) return STATUS_ERR_TRUNCATED;
    uint8_t payload[PHULJHARI_PAYLOAD_LEN_EFFECT_STOP];
    payload[0] = stop->reason;

    return serialize_frame(
        MSG_TYPE_EFFECT_STOP,
        seq,
        payload,
        PHULJHARI_PAYLOAD_LEN_EFFECT_STOP,
        out_buf,
        out_buf_capacity,
        out_frame_len
    );
}

/**
 * @brief Deserializes MSG_TYPE_EFFECT_STOP.
 */
static inline StatusCode deserialize_effect_stop(
    const RawFrame* frame,
    EffectStopMsg* out_stop
) {
    if (frame == NULL || out_stop == NULL) return STATUS_ERR_TRUNCATED;
    if (frame->msg_type != MSG_TYPE_EFFECT_STOP) return STATUS_ERR_INVALID_TYPE;
    if (frame->payload_len != PHULJHARI_PAYLOAD_LEN_EFFECT_STOP) return STATUS_ERR_PAYLOAD_LENGTH;

    out_stop->reason = frame->payload[0];
    return STATUS_OK;
}

/**
 * @brief Serializes MSG_TYPE_STATUS_RESP.
 */
static inline StatusCode serialize_status_resp(
    uint16_t seq,
    const StatusRespMsg* resp,
    uint8_t* out_buf,
    size_t out_buf_capacity,
    size_t* out_frame_len
) {
    if (resp == NULL) return STATUS_ERR_TRUNCATED;
    uint8_t payload[PHULJHARI_PAYLOAD_LEN_STATUS_RESP];
    payload[0] = resp->current_effect;
    payload[1] = resp->receiver_state;
    write_u16_be(&payload[2], resp->remaining_duration_ms);
    write_u32_be(&payload[4], resp->total_effects_executed);

    return serialize_frame(
        MSG_TYPE_STATUS_RESP,
        seq,
        payload,
        PHULJHARI_PAYLOAD_LEN_STATUS_RESP,
        out_buf,
        out_buf_capacity,
        out_frame_len
    );
}

/**
 * @brief Deserializes MSG_TYPE_STATUS_RESP.
 */
static inline StatusCode deserialize_status_resp(
    const RawFrame* frame,
    StatusRespMsg* out_resp
) {
    if (frame == NULL || out_resp == NULL) return STATUS_ERR_TRUNCATED;
    if (frame->msg_type != MSG_TYPE_STATUS_RESP) return STATUS_ERR_INVALID_TYPE;
    if (frame->payload_len != PHULJHARI_PAYLOAD_LEN_STATUS_RESP) return STATUS_ERR_PAYLOAD_LENGTH;

    out_resp->current_effect = frame->payload[0];
    out_resp->receiver_state = frame->payload[1];
    out_resp->remaining_duration_ms = read_u16_be(&frame->payload[2]);
    out_resp->total_effects_executed = read_u32_be(&frame->payload[4]);
    return STATUS_OK;
}

/* -------------------------------------------------------------------------
 * Sequence Tracking & Duplicate Rejection Filter
 * ------------------------------------------------------------------------- */
/**
 * @brief Robust sliding-window sequence deduplicator.
 * Handles 16-bit sequence number wraparound and maintains a 32-packet
 * history window for out-of-order packets.
 */
class DuplicateFilter {
public:
    DuplicateFilter() : initialized_(false), last_seq_(0), window_mask_(0) {}

    void reset() {
        initialized_ = false;
        last_seq_ = 0;
        window_mask_ = 0;
    }

    /**
     * @brief Evaluates whether a sequence number is a duplicate.
     * @param seq The sequence number to test.
     * @return true if duplicate (already seen or too old), false if new (and records it).
     */
    bool check_and_record(uint16_t seq) {
        if (!initialized_) {
            initialized_ = true;
            last_seq_ = seq;
            window_mask_ = 1U;
            return false; /* First sequence accepted */
        }

        /* 16-bit modular difference handles wraparound: 0 - 65535 = +1 */
        int16_t diff = (int16_t)(seq - last_seq_);

        if (diff == 0) {
            /* Exact match with last sequence -> duplicate */
            return true;
        } else if (diff > 0) {
            /* Newer packet than last_seq */
            if (diff < 32) {
                window_mask_ <<= diff;
                window_mask_ |= 1U;
            } else {
                /* Jumped ahead past window */
                window_mask_ = 1U;
            }
            last_seq_ = seq;
            return false;
        } else {
            /* diff < 0: Older packet, check within 31-packet sliding window */
            int16_t age = -diff;
            if (age < 32) {
                uint32_t bit = (1U << (uint32_t)age);
                if ((window_mask_ & bit) != 0) {
                    /* Bit already set: duplicate */
                    return true;
                } else {
                    /* Unseen out-of-order packet within window */
                    window_mask_ |= bit;
                    return false;
                }
            } else {
                /* Outside tracking window: considered stale / duplicate */
                return true;
            }
        }
    }

    uint16_t last_seq() const { return last_seq_; }
    bool is_initialized() const { return initialized_; }

private:
    bool initialized_;
    uint16_t last_seq_;
    uint32_t window_mask_;
};

/* -------------------------------------------------------------------------
 * Link Health Tracker
 * ------------------------------------------------------------------------- */
/**
 * @brief Manages link health based on periodic heartbeats and application ACKs.
 */
class LinkHealthTracker {
public:
    LinkHealthTracker(
        uint32_t ping_interval_ms = 2000,
        uint32_t reconnect_timeout_ms = 4500,
        uint32_t offline_timeout_ms = 9000
    ) : ping_interval_ms_(ping_interval_ms),
        reconnect_timeout_ms_(reconnect_timeout_ms),
        offline_timeout_ms_(offline_timeout_ms),
        last_rx_ms_(0),
        last_ping_tx_ms_(0),
        has_initial_rx_(false) {}

    void record_rx(uint32_t current_time_ms) {
        last_rx_ms_ = current_time_ms;
        has_initial_rx_ = true;
    }

    void record_ping_tx(uint32_t current_time_ms) {
        last_ping_tx_ms_ = current_time_ms;
    }

    bool should_send_ping(uint32_t current_time_ms) const {
        return (current_time_ms - last_ping_tx_ms_ >= ping_interval_ms_);
    }

    LinkHealth evaluate(uint32_t current_time_ms) const {
        if (!has_initial_rx_) {
            return LINK_OFFLINE;
        }
        uint32_t elapsed = current_time_ms - last_rx_ms_;
        if (elapsed < reconnect_timeout_ms_) {
            return LINK_CONNECTED;
        } else if (elapsed < offline_timeout_ms_) {
            return LINK_RECONNECTING;
        } else {
            return LINK_OFFLINE;
        }
    }

private:
    uint32_t ping_interval_ms_;
    uint32_t reconnect_timeout_ms_;
    uint32_t offline_timeout_ms_;
    uint32_t last_rx_ms_;
    uint32_t last_ping_tx_ms_;
    bool has_initial_rx_;
};

#ifdef __cplusplus
} // namespace protocol
} // namespace phuljhari
#endif

#endif /* PHULJHARI_PROTOCOL_H */
