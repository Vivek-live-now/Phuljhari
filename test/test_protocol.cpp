/**
 * @file test_protocol.cpp
 * @brief Host-native unit test suite for Project Phuljhari ✨
 * 
 * Verifies:
 * - CRC-8 calculation and bit-corruption detection
 * - Malformed and truncated frame rejection
 * - Invalid message type and invalid effect ID handling
 * - Sequence number arithmetic and 16-bit wraparound
 * - Duplicate message rejection via sliding window filter
 * - Hardware safety boundaries (duration, intensity, brightness clamp)
 * - Separation of transport status, application ACKs, and link health
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cassert>

#include "phuljhari_safety.h"
#include "phuljhari_protocol.h"

using namespace phuljhari;
using namespace phuljhari::protocol;
using namespace phuljhari::safety;

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
        return false; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    g_tests_run++; \
    std::cout << "[RUN] " << #fn << "...\n"; \
    if (fn()) { \
        g_tests_passed++; \
        std::cout << "  [PASS] " << #fn << "\n"; \
    } else { \
        g_tests_failed++; \
        std::cout << "  [FAILED] " << #fn << "\n"; \
    } \
} while(0)

/* -------------------------------------------------------------------------
 * Test 1: CRC-8 Calculation & Corruption Detection
 * ------------------------------------------------------------------------- */
static bool test_crc8_integrity() {
    uint8_t sample_data[] = { 0x50, 0x48, 0x01, 0x03, 0x00, 0x01, 0x05, 0x01, 0x13, 0x88, 0x50, 0x00 };
    size_t len = sizeof(sample_data);
    uint8_t crc1 = crc8_calculate(sample_data, len);
    uint8_t crc2 = crc8_calculate(sample_data, len);
    TEST_ASSERT(crc1 == crc2, "CRC calculation must be deterministic");
    TEST_ASSERT(crc1 != 0x00, "CRC for sample data should be non-zero");

    // Single-bit flip in every byte must produce a different CRC
    for (size_t i = 0; i < len; ++i) {
        for (int bit = 0; bit < 8; ++bit) {
            sample_data[i] ^= (1U << bit);
            uint8_t corrupted_crc = crc8_calculate(sample_data, len);
            TEST_ASSERT(corrupted_crc != crc1, "Single bit flip must alter CRC-8");
            sample_data[i] ^= (1U << bit); // restore
        }
    }
    return true;
}

/* -------------------------------------------------------------------------
 * Test 2: Frame Serialization & Deserialization (Roundtrip)
 * ------------------------------------------------------------------------- */
static bool test_frame_serialization_roundtrip() {
    uint8_t payload[5] = { 0x01, 0x0B, 0xB8, 0x50, 0x00 };
    uint8_t wire_buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t wire_len = 0;

    StatusCode status = serialize_frame(
        MSG_TYPE_EFFECT_CMD,
        1234,
        payload,
        sizeof(payload),
        wire_buf,
        sizeof(wire_buf),
        &wire_len
    );
    TEST_ASSERT(status == STATUS_OK, "Serialization failed");
    TEST_ASSERT(wire_len == PHULJHARI_HEADER_SIZE + sizeof(payload) + 1, "Incorrect wire frame length");
    TEST_ASSERT(wire_buf[0] == PHULJHARI_MAGIC_0, "Magic 0 mismatch");
    TEST_ASSERT(wire_buf[1] == PHULJHARI_MAGIC_1, "Magic 1 mismatch");
    TEST_ASSERT(wire_buf[2] == PHULJHARI_PROTO_VERSION, "Version mismatch");
    TEST_ASSERT(wire_buf[3] == MSG_TYPE_EFFECT_CMD, "MsgType mismatch");
    TEST_ASSERT(read_u16_be(&wire_buf[4]) == 1234, "Seq mismatch");
    TEST_ASSERT(wire_buf[6] == sizeof(payload), "Payload length mismatch");

    RawFrame frame;
    status = deserialize_frame(wire_buf, wire_len, &frame);
    TEST_ASSERT(status == STATUS_OK, "Deserialization failed");
    TEST_ASSERT(frame.msg_type == MSG_TYPE_EFFECT_CMD, "Deserialized type mismatch");
    TEST_ASSERT(frame.seq == 1234, "Deserialized seq mismatch");
    TEST_ASSERT(frame.payload_len == sizeof(payload), "Deserialized payload len mismatch");
    TEST_ASSERT(std::memcmp(frame.payload, payload, sizeof(payload)) == 0, "Deserialized payload contents mismatch");
    return true;
}

/* -------------------------------------------------------------------------
 * Test 3: Truncated & Malformed Frames
 * ------------------------------------------------------------------------- */
static bool test_truncated_and_malformed_frames() {
    uint8_t valid_buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t valid_len = 0;
    uint8_t payload[4] = { 0x11, 0x22, 0x33, 0x44 };

    StatusCode st = serialize_frame(MSG_TYPE_PING, 42, payload, sizeof(payload), valid_buf, sizeof(valid_buf), &valid_len);
    TEST_ASSERT(st == STATUS_OK, "Valid frame setup failed");

    RawFrame frame;

    // Buffer smaller than minimum frame size
    TEST_ASSERT(deserialize_frame(valid_buf, 4, &frame) == STATUS_ERR_TRUNCATED, "Should reject len < MIN_FRAME_SIZE");
    TEST_ASSERT(deserialize_frame(valid_buf, 0, &frame) == STATUS_ERR_TRUNCATED, "Should reject zero length");
    TEST_ASSERT(deserialize_frame(NULL, valid_len, &frame) == STATUS_ERR_TRUNCATED, "Should reject NULL pointer");

    // Frame cut off before declared payload finishes
    TEST_ASSERT(deserialize_frame(valid_buf, valid_len - 1, &frame) == STATUS_ERR_TRUNCATED, "Should reject truncated payload/CRC");

    // Corrupted magic bytes
    uint8_t bad_magic[PHULJHARI_MAX_FRAME_SIZE];
    std::memcpy(bad_magic, valid_buf, valid_len);
    bad_magic[0] = 'X';
    TEST_ASSERT(deserialize_frame(bad_magic, valid_len, &frame) == STATUS_ERR_MAGIC, "Should reject bad magic byte 0");

    bad_magic[0] = PHULJHARI_MAGIC_0;
    bad_magic[1] = 'Y';
    TEST_ASSERT(deserialize_frame(bad_magic, valid_len, &frame) == STATUS_ERR_MAGIC, "Should reject bad magic byte 1");

    // Unsupported protocol version
    uint8_t bad_version[PHULJHARI_MAX_FRAME_SIZE];
    std::memcpy(bad_version, valid_buf, valid_len);
    bad_version[2] = 0x99;
    // Recompute CRC for modified version to verify it is caught by version check, not CRC check
    bad_version[valid_len - 1] = crc8_calculate(bad_version, valid_len - 1);
    TEST_ASSERT(deserialize_frame(bad_version, valid_len, &frame) == STATUS_ERR_VERSION, "Should reject version mismatch");

    // Checksum mismatch
    uint8_t bad_crc[PHULJHARI_MAX_FRAME_SIZE];
    std::memcpy(bad_crc, valid_buf, valid_len);
    bad_crc[valid_len - 1] ^= 0x55;
    TEST_ASSERT(deserialize_frame(bad_crc, valid_len, &frame) == STATUS_ERR_CRC, "Should reject corrupted CRC");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 4: Invalid Message Types & Invalid Effect IDs
 * ------------------------------------------------------------------------- */
static bool test_invalid_types_and_effects() {
    uint8_t buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t out_len = 0;

    // Unknown message types
    TEST_ASSERT(serialize_frame(0x00, 1, NULL, 0, buf, sizeof(buf), &out_len) == STATUS_ERR_INVALID_TYPE, "Should reject msg_type 0x00");
    TEST_ASSERT(serialize_frame(0x08, 1, NULL, 0, buf, sizeof(buf), &out_len) == STATUS_ERR_INVALID_TYPE, "Should reject msg_type 0x08");
    TEST_ASSERT(serialize_frame(0xFF, 1, NULL, 0, buf, sizeof(buf), &out_len) == STATUS_ERR_INVALID_TYPE, "Should reject msg_type 0xFF");

    // Invalid Effect IDs in EffectCmd
    EffectCmd cmd;
    cmd.effect_id = 0x04; // Valid are 0 (idle), 1 (phuljhari), 2 (anar), 3 (patakha)
    cmd.duration_ms = 5000;
    cmd.intensity = 80;
    cmd.flags = 0;
    TEST_ASSERT(serialize_effect_cmd(1, &cmd, buf, sizeof(buf), &out_len) == STATUS_ERR_INVALID_EFFECT, "Should reject effect_id 0x04");

    cmd.effect_id = 0xFF;
    TEST_ASSERT(serialize_effect_cmd(1, &cmd, buf, sizeof(buf), &out_len) == STATUS_ERR_INVALID_EFFECT, "Should reject effect_id 0xFF");

    // All valid effect IDs must serialize successfully
    for (uint8_t eid = EFFECT_ID_IDLE; eid <= EFFECT_ID_PATAKHA; ++eid) {
        cmd.effect_id = eid;
        TEST_ASSERT(serialize_effect_cmd(1, &cmd, buf, sizeof(buf), &out_len) == STATUS_OK, "Valid effect ID must serialize");
    }

    return true;
}

/* -------------------------------------------------------------------------
 * Test 5: Hardware Safety Boundaries (Duration, Intensity, Brightness)
 * ------------------------------------------------------------------------- */
static bool test_safety_boundary_enforcement() {
    // Duration limits
    TEST_ASSERT(is_duration_safe(PHULJHARI_SAFETY_MIN_DURATION_MS), "Min duration should be safe");
    TEST_ASSERT(is_duration_safe(PHULJHARI_SAFETY_MAX_DURATION_MS), "Max duration should be safe");
    TEST_ASSERT(!is_duration_safe(PHULJHARI_SAFETY_MIN_DURATION_MS - 1), "Below min duration must be unsafe");
    TEST_ASSERT(!is_duration_safe(PHULJHARI_SAFETY_MAX_DURATION_MS + 1), "Above max duration must be unsafe");
    TEST_ASSERT(!is_duration_safe(0), "Zero duration must be unsafe");

    TEST_ASSERT(clamp_duration(50) == PHULJHARI_SAFETY_MIN_DURATION_MS, "Duration < min should clamp to min");
    TEST_ASSERT(clamp_duration(60000) == PHULJHARI_SAFETY_MAX_DURATION_MS, "Duration > max should clamp to max");
    TEST_ASSERT(clamp_duration(3500) == 3500, "Normal duration should remain unchanged");

    // Intensity limits
    TEST_ASSERT(is_intensity_safe(1), "1% intensity should be safe");
    TEST_ASSERT(is_intensity_safe(100), "100% intensity should be safe");
    TEST_ASSERT(!is_intensity_safe(0), "0% intensity must be unsafe");
    TEST_ASSERT(!is_intensity_safe(101), "101% intensity must be unsafe");

    TEST_ASSERT(clamp_intensity(0) == 1, "0% should clamp to 1%");
    TEST_ASSERT(clamp_intensity(255) == 100, "255% should clamp to 100%");

    // FastLED Brightness scaling strictly capped to prevent USB brownout
    TEST_ASSERT(intensity_to_safe_brightness(100) == PHULJHARI_SAFETY_MAX_LED_BRIGHTNESS, "100% intensity must equal max safe brightness");
    TEST_ASSERT(intensity_to_safe_brightness(50) == PHULJHARI_SAFETY_MAX_LED_BRIGHTNESS / 2, "50% intensity scaling");
    TEST_ASSERT(intensity_to_safe_brightness(255) <= PHULJHARI_SAFETY_MAX_LED_BRIGHTNESS, "Overflow intensity must not exceed max brightness");

    // Output pin state defaults
    TEST_ASSERT(PHULJHARI_OUTPUT_STATE_INACTIVE == 0, "Inactive safe state must be 0 (LOW)");

    // Test rejection during serialization
    uint8_t buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t len = 0;
    EffectCmd bad_cmd = { EFFECT_ID_PHULJHARI, 20000 /* >15s */, 50, 0 };
    TEST_ASSERT(serialize_effect_cmd(1, &bad_cmd, buf, sizeof(buf), &len) == STATUS_ERR_SAFETY_VIOLATION, "Must reject >15s duration");

    bad_cmd.duration_ms = 5000;
    bad_cmd.intensity = 0; // Invalid 0%
    TEST_ASSERT(serialize_effect_cmd(1, &bad_cmd, buf, sizeof(buf), &len) == STATUS_ERR_SAFETY_VIOLATION, "Must reject 0% intensity");

    bad_cmd.intensity = 150; // Invalid >100%
    TEST_ASSERT(serialize_effect_cmd(1, &bad_cmd, buf, sizeof(buf), &len) == STATUS_ERR_SAFETY_VIOLATION, "Must reject >100% intensity");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 6: Sequence Number Wraparound & Duplicate Message Rejection
 * ------------------------------------------------------------------------- */
static bool test_sequence_wraparound_and_duplicates() {
    DuplicateFilter filter;
    TEST_ASSERT(!filter.is_initialized(), "Filter should start uninitialized");

    // 1. Initial sequence accepted
    TEST_ASSERT(!filter.check_and_record(100), "Initial sequence 100 should be accepted");
    TEST_ASSERT(filter.is_initialized(), "Filter should be initialized");
    TEST_ASSERT(filter.last_seq() == 100, "Last sequence should be 100");

    // 2. Exact duplicate sequence rejected
    TEST_ASSERT(filter.check_and_record(100), "Exact duplicate 100 must be rejected");

    // 3. Monotonic advance
    TEST_ASSERT(!filter.check_and_record(101), "Sequence 101 should be accepted");
    TEST_ASSERT(filter.check_and_record(101), "Duplicate 101 must be rejected");
    TEST_ASSERT(filter.check_and_record(100), "Earlier seen 100 must be rejected");

    // 4. Out-of-order within sliding window
    TEST_ASSERT(!filter.check_and_record(105), "Jump ahead to 105 accepted");
    TEST_ASSERT(!filter.check_and_record(103), "Delayed in-window 103 accepted first time");
    TEST_ASSERT(filter.check_and_record(103), "Delayed in-window 103 rejected second time");
    TEST_ASSERT(!filter.check_and_record(102), "Delayed in-window 102 accepted first time");
    TEST_ASSERT(filter.check_and_record(102), "Delayed in-window 102 rejected second time");

    // 5. Packets older than 31 positions behind window head are rejected as stale
    TEST_ASSERT(!filter.check_and_record(150), "Jump ahead to 150 accepted");
    TEST_ASSERT(filter.check_and_record(104), "Stale packet (< 150-31) must be rejected");

    // 6. 16-bit Wraparound test: 65534 -> 65535 -> 0 -> 1 -> 2
    filter.reset();
    TEST_ASSERT(!filter.check_and_record(65534), "Seq 65534 accepted");
    TEST_ASSERT(!filter.check_and_record(65535), "Seq 65535 accepted");
    TEST_ASSERT(filter.check_and_record(65535), "Duplicate 65535 rejected");

    // Wraparound to 0
    TEST_ASSERT(!filter.check_and_record(0), "Wrapped seq 0 accepted as newer than 65535");
    TEST_ASSERT(filter.last_seq() == 0, "Last seq should now be 0");
    TEST_ASSERT(filter.check_and_record(0), "Duplicate wrapped 0 rejected");
    TEST_ASSERT(filter.check_and_record(65535), "Previous seq 65535 now rejected as duplicate");

    // Advance to 1 and 2
    TEST_ASSERT(!filter.check_and_record(1), "Seq 1 accepted");
    TEST_ASSERT(!filter.check_and_record(2), "Seq 2 accepted");
    TEST_ASSERT(filter.check_and_record(1), "Duplicate 1 rejected");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 7: Separation of Transport Status, App ACKs, and Link Health
 * ------------------------------------------------------------------------- */
static bool test_transport_ack_and_link_separation() {
    // 1. Verify TransportStatus enum values are distinct
    TransportStatus tx_idle = TRANSPORT_TX_IDLE;
    TransportStatus tx_succ = TRANSPORT_TX_SUCCESS;
    TransportStatus tx_fail = TRANSPORT_TX_FAIL;
    TEST_ASSERT(tx_idle != tx_succ && tx_succ != tx_fail, "Transport statuses must be distinct");

    // 2. Application-Level ACK test
    EffectAck ack_msg;
    ack_msg.ack_seq = 4096;
    ack_msg.status_code = STATUS_OK;
    ack_msg.receiver_state = RECV_STATE_EFFECT_ACTIVE;
    ack_msg.remaining_ms = 4800;

    uint8_t ack_wire[PHULJHARI_MAX_FRAME_SIZE];
    size_t ack_len = 0;
    StatusCode st = serialize_effect_ack(999, &ack_msg, ack_wire, sizeof(ack_wire), &ack_len);
    TEST_ASSERT(st == STATUS_OK, "Effect ACK serialization failed");

    RawFrame ack_frame;
    st = deserialize_frame(ack_wire, ack_len, &ack_frame);
    TEST_ASSERT(st == STATUS_OK, "Effect ACK raw deserialization failed");

    EffectAck unpacked_ack;
    st = deserialize_effect_ack(&ack_frame, &unpacked_ack);
    TEST_ASSERT(st == STATUS_OK, "Effect ACK unpacked deserialization failed");
    TEST_ASSERT(unpacked_ack.ack_seq == 4096, "ACK sequence mismatch");
    TEST_ASSERT(unpacked_ack.status_code == STATUS_OK, "ACK status code mismatch");
    TEST_ASSERT(unpacked_ack.receiver_state == RECV_STATE_EFFECT_ACTIVE, "ACK state mismatch");
    TEST_ASSERT(unpacked_ack.remaining_ms == 4800, "ACK remaining duration mismatch");

    // 3. Heartbeat-Based Link Health Monitor
    LinkHealthTracker link(2000, 4500, 9000);
    // Initial state without any RX is OFFLINE
    TEST_ASSERT(link.evaluate(0) == LINK_OFFLINE, "Initial link state must be OFFLINE");

    // Receiver responds at t = 1000ms
    link.record_rx(1000);
    TEST_ASSERT(link.evaluate(1500) == LINK_CONNECTED, "Active link should be CONNECTED");
    TEST_ASSERT(link.evaluate(5000) == LINK_CONNECTED, "At 4000ms elapsed (<4500ms), still CONNECTED");

    // Time elapsed reaches 6000ms (5000ms elapsed since last rx at 1000ms) -> RECONNECTING
    TEST_ASSERT(link.evaluate(6000) == LINK_RECONNECTING, "At 5000ms elapsed, state should transition to RECONNECTING");

    // Time elapsed reaches 11000ms (10000ms elapsed since last rx) -> OFFLINE
    TEST_ASSERT(link.evaluate(11000) == LINK_OFFLINE, "At 10000ms elapsed (>9000ms), state should transition to OFFLINE");

    // Heartbeat recovered at t = 11500ms -> back to CONNECTED
    link.record_rx(11500);
    TEST_ASSERT(link.evaluate(12000) == LINK_CONNECTED, "Recovered heartbeat should restore LINK_CONNECTED");

    return true;
}

/* -------------------------------------------------------------------------
 * Test 8: All Message Serializers and Deserializers
 * ------------------------------------------------------------------------- */
static bool test_all_typed_messages() {
    uint8_t buf[PHULJHARI_MAX_FRAME_SIZE];
    size_t len = 0;
    RawFrame frame;

    // Ping
    PingMsg ping_out = { 1234567 };
    TEST_ASSERT(serialize_ping(10, &ping_out, buf, sizeof(buf), &len) == STATUS_OK, "Ping serialize");
    TEST_ASSERT(deserialize_frame(buf, len, &frame) == STATUS_OK, "Ping frame deserialize");
    PingMsg ping_in;
    TEST_ASSERT(deserialize_ping(&frame, &ping_in) == STATUS_OK, "Ping msg deserialize");
    TEST_ASSERT(ping_in.uptime_ms == 1234567, "Ping uptime match");

    // Pong
    PongMsg pong_out = { 9876543, RECV_STATE_SAFE_IDLE };
    TEST_ASSERT(serialize_pong(11, &pong_out, buf, sizeof(buf), &len) == STATUS_OK, "Pong serialize");
    TEST_ASSERT(deserialize_frame(buf, len, &frame) == STATUS_OK, "Pong frame deserialize");
    PongMsg pong_in;
    TEST_ASSERT(deserialize_pong(&frame, &pong_in) == STATUS_OK, "Pong msg deserialize");
    TEST_ASSERT(pong_in.uptime_ms == 9876543, "Pong uptime match");
    TEST_ASSERT(pong_in.receiver_state == RECV_STATE_SAFE_IDLE, "Pong state match");

    // Effect Stop
    EffectStopMsg stop_out = { 1 };
    TEST_ASSERT(serialize_effect_stop(12, &stop_out, buf, sizeof(buf), &len) == STATUS_OK, "Stop serialize");
    TEST_ASSERT(deserialize_frame(buf, len, &frame) == STATUS_OK, "Stop frame deserialize");
    EffectStopMsg stop_in;
    TEST_ASSERT(deserialize_effect_stop(&frame, &stop_in) == STATUS_OK, "Stop msg deserialize");
    TEST_ASSERT(stop_in.reason == 1, "Stop reason match");

    // Status Resp
    StatusRespMsg resp_out = { EFFECT_ID_ANAR, RECV_STATE_EFFECT_ACTIVE, 3200, 42 };
    TEST_ASSERT(serialize_status_resp(13, &resp_out, buf, sizeof(buf), &len) == STATUS_OK, "Status resp serialize");
    TEST_ASSERT(deserialize_frame(buf, len, &frame) == STATUS_OK, "Status resp frame deserialize");
    StatusRespMsg resp_in;
    TEST_ASSERT(deserialize_status_resp(&frame, &resp_in) == STATUS_OK, "Status resp msg deserialize");
    TEST_ASSERT(resp_in.current_effect == EFFECT_ID_ANAR, "Status effect match");
    TEST_ASSERT(resp_in.receiver_state == RECV_STATE_EFFECT_ACTIVE, "Status state match");
    TEST_ASSERT(resp_in.remaining_duration_ms == 3200, "Status duration match");
    TEST_ASSERT(resp_in.total_effects_executed == 42, "Status total match");

    return true;
}

/* -------------------------------------------------------------------------
 * Main Test Runner
 * ------------------------------------------------------------------------- */
int main() {
    std::cout << "====================================================\n";
    std::cout << "Project Phuljhari ✨ Native Protocol & Safety Tests\n";
    std::cout << "====================================================\n";

    RUN_TEST(test_crc8_integrity);
    RUN_TEST(test_frame_serialization_roundtrip);
    RUN_TEST(test_truncated_and_malformed_frames);
    RUN_TEST(test_invalid_types_and_effects);
    RUN_TEST(test_safety_boundary_enforcement);
    RUN_TEST(test_sequence_wraparound_and_duplicates);
    RUN_TEST(test_transport_ack_and_link_separation);
    RUN_TEST(test_all_typed_messages);

    std::cout << "====================================================\n";
    std::cout << "Test Summary: " << g_tests_passed << "/" << g_tests_run << " passed";
    if (g_tests_failed > 0) {
        std::cout << " (" << g_tests_failed << " FAILED)";
    }
    std::cout << "\n====================================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
